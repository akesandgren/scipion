#!/usr/bin/env python
#------------------------------------------------------------------------------------------------
# General script (part A) for Xmipp-based manual particle picking
#
# For each micrograph in the MicrographSelfile, this program will launch
# the xmipp_mark program 
# A graphical interface exists to identify micrographs that have been finished
#
# Example use:
# python xmipp_particle_pick.py &
#
# Author: Sjors Scheres, March 2007
#
#------------------------------------------------------------------------------------------------
# {section} Global parameters
#------------------------------------------------------------------------------------------------
# {dir} Working subdirectory:
""" Use the same directory where you executed xmipp_protocol_preprocess_micrographs.py
"""
WorkingDir='Preprocessing'
# {file} Selfile with all micrographs to pick particles from:
MicrographSelfile='Preprocessing/all_micrographs.sel'
# Is this selfile a list of untilted-tilted pairs?
""" True for RCT-processing. In that case, provide a 3-column selfile as follows:
    untilted_pair1.raw tilted_pair1.raw 1
    untilted_pair2.raw tilted_pair2.raw 1
    etc...
    Where 1 in the third column means active pair, and -1 means inactive pair
    Use relative paths from the Preprocessing directory!
"""
IsPairList=False

# Name of the position files (or family name)
""" This is specified inside the micrograph_mark program (Common by default)
"""
PosName='Common'

# Perform automatic particle picking
""" Perform automatic particle picking """
AutomaticPicking=True

# {expert} Name of the model for automatic particle picking
""" Different names can be used to distinguish between particle types
"""
AutomaticName='automodel'

# {expert} Root directory name for this project:
""" Absolute path to the root directory for this project
"""
ProjectDir='/home/coss/temp/Far_from_focus'
# {expert} Directory name for logfiles:
LogDir='Logs'

#------------------------------------------------------------------------------------------------
# {section} Parallelization issues for automatic particle picking
#------------------------------------------------------------------------------------------------
# Number of (shared-memory) threads?
""" This option provides shared-memory parallelization on multi-core machines. 
    It does not require any additional software, other than xmipp
"""
NumberOfThreads=1
# This script has been parallelized at python level
""" This python script can be run as a parallel job,
    In order to use several CPUs you will need to
    answer yes to the "Use a job queing system" pop-up
    window and after the queueing command add
    mpirun -np N -machinefile machine.txt 
    or whatever is adecuate in your computer
    (N is the number of CPUs).  Each process will create a
    different log file. The names of these files are the
    same as the ones used by the sequential program plus the process number
"""
UselessVariable='read help'

# {expert} Abort if mpi is not installed?
"""If set to True the script will abort if mpi4py is not installed and one executes this script through mpirun
"""
CheckMpi=False

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
# {end-of-header} USUALLY YOU DO NOT NEED TO MODIFY ANYTHING BELOW THIS LINE ...
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
#
#Next lines will import mpi4py package, if import fails a dummy 
#mpi class will be created. 
#In this way the same file may be use for mpi and
#serial jobs  
try:
    from mpi4py import MPI
    mpi = MPI.COMM_WORLD
    mpiActive=True
except ImportError:
    class DummyMPI:
        '''A dummy MPI class for running serial jobs.'''
        def Get_rank(dummy):
            return 0
        def Get_size(dummy):
            return 1
        def Get_processor_name(dummy):
            return 'localhost' 
        def Barrier(dummy):
            return 1
        def Bcast(sendbuffer):
            return sendbuffer
    MPI = DummyMPI()
    mpi = DummyMPI()
    mpiActive=False
#
#
from Tkinter import *
# Create a GUI automatically from a selfile of micrographs
class particle_pick_class:

    def __init__(self,
                 WorkingDir,
                 MicrographSelfile,
                 IsPairList,
                 PosName,
                 AutomaticPicking,
                 AutomaticName,
                 ProjectDir,
                 LogDir,
                 NumberOfThreads,
                 CheckMpi
                 ):

        import os,sys
        scriptdir=os.path.split(os.path.dirname(os.popen('which xmipp_protocols','r').read()))[0]+'/protocols'
        sys.path.append(scriptdir) # add default search path
        self.SYSTEMSCRIPTDIR=scriptdir
        import log

        self.WorkingDir=WorkingDir
        self.MicrographSelfile=os.path.abspath(MicrographSelfile)
        self.IsPairList=IsPairList
        self.PosName=PosName
        self.AutomaticPicking=AutomaticPicking
        self.AutomaticName=AutomaticName
        self.ProjectDir=ProjectDir
        self.LogDir=LogDir
        self.NumberOfThreads=NumberOfThreads
        self.myrank = mpi.Get_rank()
        self.nprocs = mpi.Get_size()

        # Setup logging
        self.log=log.init_log_system(self.ProjectDir,
                                     LogDir,
                                     sys.argv[0]+"_"+str(self.myrank),
                                     self.WorkingDir)
                
        # Check MPI
        if (mpiActive):
            print "Python has mpi4py package, python parallel jobs are possible"
            self.log.info("Python has mpi4py package, python parallel jobs are possible")        
        else:
            print "Python has NOT mpi4py package, python parallel jobs are NOT possible"
            self.log.info("Python has NOT mpi4py package, python parallel jobs are NOT possible")
            if (CheckMpi):
                print "mpi is not installed and checkmpi flag is set to True"
                print "disable mpi checking flag in the script" 
                print "and launch it as a serial job" 
                print "that is, without mpirun" 
                exit(1)

        # Make working directory if it does not exist yet
        if (self.myrank==0):
            if not os.path.exists(self.WorkingDir):
                os.makedirs(self.WorkingDir)

            # Backup script
            log.make_backup_of_script_file(sys.argv[0],
                                           os.path.abspath(self.WorkingDir))
    
        # Execute protocol in the working directory
        os.chdir(self.WorkingDir)
        
        # Execute protocol in the working directory
        if (self.myrank==0):
            self.print_warning()
            self.sellines=self.ReadSelfile()
            self.MakeGui()
        else:
            self.AutomaticallyDetect()

        # Return to parent dir
        os.chdir(os.pardir)

    def MakeGui(self):
        import xmipp_protocol_gui

        self.master=Tk()
        self.total_count=0
        self.total_count_auto=0
        self.whichmark=StringVar()
        self.whichtilted={}
        self.row={}

        # Create the Canvas with Scrollbars
        self.canvas,self.frame=xmipp_protocol_gui.PrepareCanvas(self.master)

        # Fill the GUI
        self.FillMarkGui()

        # Launch the window
        xmipp_protocol_gui.LaunchCanvas(self.master,self.canvas,self.frame)
        xmipp_protocol_gui.GuiResize(self.master,self.frame)

        # Enter main loop
        self.master.mainloop()

    def ReadSelfile(self):
        import os
        newlines=[]
        if not os.path.exists(self.MicrographSelfile):
            message='Error: '+self.MicrographSelfile+' does not exist'
            print '*',message
            self.log.error(message)
            sys.exit()
        fh=open(self.MicrographSelfile,'r')
        lines=fh.readlines()
        fh.close()
        words=lines[0].split()
        if ((not self.IsPairList) and (not len(words)==2)):
            message='Error: '+self.MicrographSelfile+' is not a valid selection file'
            print '*',message
            self.log.error(message)
            sys.exit()
        if ((self.IsPairList) and (not len(words)==3)):
            message='Error: '+self.MicrographSelfile+' is not a valid pairlist file'
            print '*',message
            self.log.error(message)
            sys.exit()
        for line in lines:
            words=line.split()
            newlines.append(words)
        return newlines
 
    def FillMarkGui(self):
        import os

        # Script title
        self.master.title("GUI for particle picking")
        headertext='GUI for Xmipp particle picking\n'
        headertext+="Executed in directory: "+str(os.getcwd())
        l1=Label(self.frame, text=headertext, fg="medium blue")
        if (self.AutomaticPicking):
            l1.grid(row=0, column=0,columnspan=5,sticky=EW)
            Label(self.frame, text="Manual").grid(row=1,column=3)
            Label(self.frame, text="Auto").grid(row=1,column=4)
        else:
            l1.grid(row=0, column=0,columnspan=3,sticky=EW)
 
        total=0
        total_auto=0
        self.selectedForAutomaticPicking=[]
        self.selectedForAutomaticPickingName=[]
        if not self.IsPairList:
            for micrograph,state in self.sellines:
                micrograph=micrograph.replace('.spi','.raw')
                if (state.find('-1') == -1):
                    c=self.CountPicked(micrograph,str(self.PosName))
                    cauto=self.CountPicked(micrograph,'auto')
                    total+=c
                    total_auto+=cauto
                    row=self.GuiAddSingleMarkEntry(micrograph,c,cauto)
                    self.row[micrograph]=row
        else:
            for micrograph,tilted,state in self.sellines:
                micrograph=micrograph.replace('.spi','.raw')
                tilted=tilted.replace('.spi','.raw')
                if (state.find('-1') == -1):
                    c=self.CountPicked(micrograph,str(self.PosName))
                    total+=c
                    self.whichtilted[micrograph]=tilted
                    row=self.GuiAddPairMarkEntry(micrograph,c)
                    self.row[micrograph]=row

        row=(self.frame.grid_size()[1]+1)
        Label(self.frame,text="").grid(row=row)
        l2=Frame(self.frame, height=2, bd=1, bg="medium blue",relief=RIDGE)
        l2.grid(row=row+1, column=0,columnspan=6,sticky=EW)
        Label(self.frame,text="").grid(row=row+2)
        self.buttonrow=(self.frame.grid_size()[1]+1)

        b = Button(self.frame, text="Close", command=self.GuiClose,underline=0)
        b.grid(row=self.buttonrow,column=0,sticky=W)
        self.master.bind('<Control_L><c>', self.GuiClose)

        if (self.AutomaticPicking):
            b = Button(self.frame, text="Invert Selection", command=self.InvertSelection,underline=0)
            b.grid(row=self.buttonrow,column=1,sticky=N+S+W+E)
            nextColumn=2
        else:
            nextColumn=1

        b = Button(self.frame, text="Update Total Count:", command=self.GuiUpdateCount)
        b.grid(row=self.buttonrow,column=nextColumn)
        nextColumn+=1

        label=str(total).zfill(5)
        l = Label(self.frame, text=label)
        l.grid(row=self.buttonrow,column=nextColumn)
        nextColumn+=1
        
        if (self.AutomaticPicking):
            label=str(total_auto).zfill(5)
            l = Label(self.frame, text=label)
            l.grid(row=self.buttonrow,column=nextColumn)

            b = Button(self.frame, text="Automatically\nDetect",
                command=self.AutomaticallyDetect,underline=0)
            b.grid(row=self.buttonrow+1,column=1,sticky=N+S+W+E)
        
    def GuiAddSingleMarkEntry(self,micrograph,count,count_auto):
        import os
        row=self.frame.grid_size()[1]

        label=os.path.basename(micrograph)
        l=Label(self.frame, text=label)
        l.grid(row=row, column=0, sticky=E)

        if (self.AutomaticPicking):
            control = IntVar()
            c=Checkbutton(self.frame, text="Auto", variable=control)
            c.grid(row=row, column=1, sticky=N+S+W+E)
            self.selectedForAutomaticPicking.append(control)
            self.selectedForAutomaticPickingName.append(micrograph)
            nextColumn=2
        else:
            nextColumn=1

        r=Radiobutton(self.frame,text="Mark!",variable=self.whichmark,
                           value=micrograph,indicatoron=0,
                           command=self.LaunchSingleMark)
        r.grid(row=row, column=nextColumn,sticky=N+S+W+E)
        nextColumn+=1

        label=str(count).zfill(5)
        l=Label(self.frame, text=label)
        l.grid(row=row, column=nextColumn, sticky=N+S+W+E)
        nextColumn+=1

        if (self.AutomaticPicking):
            label=str(count_auto).zfill(5)
            l=Label(self.frame, text=label)
            l.grid(row=row, column=nextColumn, sticky=N+S+W+E)
        return row

    def GuiAddPairMarkEntry(self,micrograph,count):
        import os
        row=self.frame.grid_size()[1]
        label=os.path.basename(micrograph)+' : '+os.path.basename(self.whichtilted[micrograph])
        l=Label(self.frame, text=label)
        l.grid(row=row, column=0, sticky=E)
        label=str(count).zfill(5)
        l=Label(self.frame, text=label)
        l.grid(row=row, column=3)
        r=Radiobutton(self.frame,text="Mark!",variable=self.whichmark,
                           value=micrograph, indicatoron=0,
                           command=self.LaunchPairMark)
        r.grid(row=row, column=2,sticky=N)
        return row

    def InvertSelection(self):
        for i in range(0,len(self.selectedForAutomaticPicking)):
            self.selectedForAutomaticPicking[i].set(1-
                self.selectedForAutomaticPicking[i].get());

    def AutomaticallyDetect(self):
        sendbuffer=[]
        if self.myrank==0:
            micrographs=[]
            for i in range(0,len(self.selectedForAutomaticPicking)):
                if (self.selectedForAutomaticPicking[i].get()):
                    micrographs.append(self.selectedForAutomaticPickingName[i])
            sendbuffer="*".join(map(str, micrographs))
        if mpiActive:
            sendbuffer = MPI.WORLD[0].Bcast(sendbuffer)
        
        micrographs=sendbuffer.split('*')
        for i in range(len(micrographs)):
            if ((i+1)%self.nprocs == self.myrank):
                self.perform_picking(micrographs[i],True)

    def CountPicked(self,micrograph,label):
        import os
        posfile=str(micrograph)+'.'+label+'.pos'
        if os.path.exists(posfile):
            fh=open(posfile,'r')
            lines=fh.readlines()
            fh.close()
            picked=len(lines)-1
            if picked>0:
                return picked
        return 0
    
    def CountAll(self, family=''):
        total=0
        for mic,row in self.row.items():
            if family=='':
                c=self.CountPicked(mic,str(self.PosName))
            else:
                c=self.CountPicked(mic,family)
            total=total+c
            label=str(c).zfill(5)
            l=Label(self.frame, text=label)
            if (self.AutomaticPicking):
                if family=='':
                    l.grid(row=row, column=3)
                else:
                    l.grid(row=row, column=4)
            else:
                l.grid(row=row, column=2)
        return total
        
    def LaunchSingleMark(self):
        import os
        self.GuiUpdateCount()
        print "* Marking... "
        self.perform_picking(self.whichmark.get(),False)

    def LaunchPairMark(self):
        import os
        self.GuiUpdateCount()
        print "* Marking... "
        untilted=self.whichmark.get()
        self.perform_picking_pair(self.whichmark.get(),self.whichtilted[self.whichmark.get()])

    def GuiUpdateCount(self):
        print "* Updating count..."
        total=self.CountAll()
        label=str(total).zfill(5)
        l=Label(self.frame, text=label)
        if (self.AutomaticPicking):
            l.grid(row=self.buttonrow, column=3)
        else:
            l.grid(row=self.buttonrow, column=2)
        if (self.AutomaticPicking):
            totalAuto=self.CountAll('auto')
            label=str(totalAuto).zfill(5)
            l=Label(self.frame, text=label)
            l.grid(row=self.buttonrow, column=4)

    def GuiClose(self):
        import sys
        self.master.quit()
        self.master.destroy()
        sys.exit(0)
        
    def print_warning(self):
        import os, sys
        print '*********************************************************************'
        print '*  Perform manual particle picking for micrographs in: '+os.path.basename(self.MicrographSelfile)
        print '*'
        print '* DONT FORGET TO SAVE YOUR COORDINATES REGULARLY, AND ALWAYS BEFORE CLOSING!'
        if (self.IsPairList):
            print '* AND ALSO SAVE THE ANGLES IN THE UNTILTED MICROGRAPHS!'
            if (self.AutomaticPicking):
                print '\n'
                print 'ERROR: CANNOT DO AUTOMATIC PICKING IN TILTED PAIRS\n'
                sys.exit(1)
        print '*'

    def perform_picking(self,name,autoSelect):
        import os
        import launch_job
        if name=='':
            return
        
        directory,micrograph=os.path.split(name)
        if (len(directory)>0):
            os.chdir(directory)
        arguments='-i '+micrograph
        if (self.AutomaticPicking):
            arguments+=' -auto ../'+self.AutomaticName
            if (self.NumberOfThreads>1):
                arguments+=' -thr '+str(self.NumberOfThreads)
            if (autoSelect):
                arguments+=' -autoSelect'
            else:
                arguments+=' &'
        launch_job.launch_job("xmipp_micrograph_mark",
                              arguments,
                              self.log,
                              False,1,1,'')
        os.chdir(os.pardir)

    def perform_picking_pair(self,untilted,tilted):
        import os
        import launch_job
        directory,uname=os.path.split(untilted)
        if (len(directory)>0):
            os.chdir(directory)
        tname='../'+tilted
        command=' -i '+uname+' -tilted '+tname + ' &'
        launch_job.launch_job("xmipp_micrograph_mark",
                              command,
                              self.log,
                              False,1,1,'')
        os.chdir(os.pardir)


    def close(self):
        message=" Exiting ... "
        print '* ',message
        print '*********************************************************************'
        self.log.info(message)
#		
# Main
#     
if __name__ == '__main__':

   	# create preprocess_A_class object

	particle_pick=particle_pick_class(WorkingDir,
                                          MicrographSelfile,
                                          IsPairList,
                                          PosName,
                                          AutomaticPicking,
                                          AutomaticName,
                                          ProjectDir,
                                          LogDir,
                                          NumberOfThreads,
                                          CheckMpi
                                          )

	# close 
	particle_pick.close()

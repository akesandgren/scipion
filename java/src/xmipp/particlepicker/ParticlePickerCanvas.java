package xmipp.particlepicker;

import ij.ImagePlus;
import ij.gui.ImageWindow;

import java.awt.Color;
import java.awt.Cursor;
import java.awt.Graphics2D;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.Toolkit;
import java.awt.event.KeyEvent;
import java.awt.event.KeyListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseMotionListener;
import java.awt.event.MouseWheelEvent;
import java.util.logging.Level;

import xmipp.ij.commons.XmippImageCanvas;
import xmipp.jni.Program;
import xmipp.particlepicker.training.model.TrainingParticle;
import xmipp.particlepicker.training.model.TrainingPicker;
import xmipp.utils.XmippResource;
import xmipp.utils.XmippWindowUtil;

public abstract class ParticlePickerCanvas extends XmippImageCanvas
{

	protected ImageWindow iw;
	
	private static boolean tongleSetSelected=false;
	

	public ParticlePickerCanvas(ImagePlus imp)
	{
		super(imp);
		addKeyListener(new KeyListener()
		{

			@Override
			public void keyTyped(KeyEvent arg0)
			{
			}

			@Override
			public void keyReleased(KeyEvent arg0)
			{
			}

			@Override
			public void keyPressed(KeyEvent e)
			{

				TrainingParticle active = getActive();
				if (active == null)
					return;
				int step = 1;
				int code = e.getKeyCode();
				if (code == KeyEvent.VK_UP)
					moveActiveParticle(active.getX(), active.getY() - step);
				else if (code == KeyEvent.VK_DOWN)
					moveActiveParticle(active.getX(), active.getY() + step);
				else if (code == KeyEvent.VK_LEFT)
					moveActiveParticle(active.getX() - step, active.getY());
				else if (code == KeyEvent.VK_RIGHT)
					moveActiveParticle(active.getX() + step, active.getY());
				else if (code == KeyEvent.VK_SPACE){
					getFrame().circlechb.setSelected(tongleSetSelected);
					getFrame().rectanglechb.setSelected(tongleSetSelected);
				        tongleSetSelected = ! tongleSetSelected;
					}
				else 
				     return;//do not repaint if not needed
				repaint();

			}
		});
		addMouseMotionListener(new MouseMotionListener() {
		    @Override
		    public void mouseMoved(MouseEvent e) {
		    	if(!getFrame().isEraserMode())
		    		return;
		    	
		        final int x = e.getX();
		        final int y = e.getY();
		        // only display a hand if the cursor is over the items
		        final Rectangle cellBounds = getBounds();
		        Toolkit toolkit = Toolkit.getDefaultToolkit();
		        Cursor eraserCursor = toolkit.createCustomCursor(XmippResource.getImage("clean.gif"), new Point(5, 5), "Eraser");
		        if (cellBounds != null && cellBounds.contains(x, y)) {
		            setCursor(eraserCursor);
		        } else {
		            setCursor(new Cursor(Cursor.DEFAULT_CURSOR));
		        }
		        
		    }
		    

		    @Override
		    public void mouseDragged(MouseEvent e) {
		    }
		});
	}
	
	protected void refresh()
	{
		getFrame().updateMicrographsModel();
		getFrame().setChanged(true);
		repaint();
	
	}
	
	public void display()
	{
		if(iw != null && iw.isVisible())
		{
			iw.setImage(getImage());
			iw.updateImage(getImage());
		}
		else
			this.iw = new ImageWindow(getImage(), this);//if you dont provide iw, I init mine
		iw.setTitle(getMicrograph().getName());
		//iw.maximize();
		iw.pack();
	}
	
	public void display(float xlocation, float ylocation)
	{
		boolean relocate = (iw == null);
			
		display();
		if(relocate)
			XmippWindowUtil.setLocation(xlocation, ylocation, iw);
	}
	
	
	public ImageWindow getIw()
	{
		return iw;
	}



	public void setIw(ImageWindow iw)
	{
		this.iw = iw;
	}



	@Override
	public void mouseWheelMoved(MouseWheelEvent e)
	{
		super.mouseWheelMoved(e);
		if(e.isShiftDown() )//zoom change detected	
			getFrame().displayZoom();
	}

	public void moveTo(TrainingParticle p)
	{
		int width = (int) getSrcRect().getWidth();
		int height = (int) getSrcRect().getHeight();
		int x0 = p.getX() - width / 2;
		if (x0 < 0)
			x0 = 0;
		if (x0 + width > imp.getWidth())
			x0 = imp.getWidth() - width;
		int y0 = p.getY() - height / 2;
		if (y0 < 0)
			y0 = 0;
		if (y0 + height > imp.getHeight())
			y0 = imp.getHeight() - height;
		Rectangle r = new Rectangle(x0, y0, width, height);
		if (!getSrcRect().contains(r))
		{
			setSourceRect(r);
			repaint();
		}
	}

	public void mouseEntered(MouseEvent e)
	{
		super.mouseEntered(e);
		setCursor(crosshairCursor);
	}

	public void mouseMoved(MouseEvent e)
	{
		super.mouseMoved(e);
		setCursor(crosshairCursor);
	}

	public abstract void refreshActive(TrainingParticle p);

	public abstract TrainingParticle getActive();

	public abstract ParticlePickerJFrame getFrame();

	protected void drawShape(Graphics2D g2, TrainingParticle p, boolean all)
	{
		
		
		int x0 = (int) getSrcRect().getX();
		int y0 = (int) getSrcRect().getY();
		int size = (int) (p.getFamily().getSize() * magnification);
		int radius = (int) (p.getFamily().getSize() / 2 * magnification);
		int x = (int) ((p.getX() - x0) * magnification);
		int y = (int) ((p.getY() - y0) * magnification);
		int distance = (int) (10 * magnification);

		if (getFrame().isShapeSelected(Shape.Rectangle) || all)
			g2.drawRect(x - radius, y - radius, size, size);
		if (getFrame().isShapeSelected(Shape.Circle) || all)
			g2.drawOval(x - radius, y - radius, size, size);
		if (getFrame().isShapeSelected(Shape.Center) || all)
		{
			g2.drawLine(x, y - distance, x, y + distance);
			g2.drawLine(x + distance, y, x - distance, y);
		}
		
	}

	protected void drawLine(double alpha, Graphics2D g2)
	{
		int width = imp.getWidth();
		int height = imp.getHeight();
		double m = 0;
		double x1, y1, x2, y2;
		if (alpha != Math.PI / 2)
		{
			m = Math.tan(alpha - Math.PI/2);

			double y = height / 2.f;
			double x = y / m;

			if (Math.abs(x) > width / 2.f)// cuts in image sides
			{
				x1 = width;// on image
				y1 = getYOnImage(m, width / 2.f);
				x2 = 0;
				y2 = getYOnImage(m, -width / 2.f);
			}
			else
			// cuts in image top and bottom
			{
				y1 = 0;
				x1 = getXOnImage(m, height / 2.f);
				y2 = height;
				x2 = getXOnImage(m, -height / 2.f);
			}
		}
		else
		{
			x1 = 0;
			y1 = y2 = height / 2.f;
			x2 = width;
			
		}
		Color ccolor = g2.getColor();
		g2.setColor(Color.yellow);
		g2.drawLine((int) (x1 * magnification), (int) (y1 * magnification), (int) (x2 * magnification), (int) (y2 * magnification));
		g2.setColor(ccolor);
	}

	private double getYOnImage(double m, double x)
	{
		int height = imp.getHeight();
		return height / 2.f - m * x;
	}

	private double getXOnImage(double m, double y)
	{
		int width = imp.getWidth();
		return y / m + width / 2.f;
	}

	public void updateMicrographData()
	{
		Micrograph m = getMicrograph();
		imp = m.getImagePlus(getFrame().getParticlePicker().getFilters());//for xmipp smooth filter
		m.runImageJFilters(getFrame().getParticlePicker().getFilters());
	}

	public abstract Micrograph getMicrograph();

	protected void moveActiveParticle(int x, int y)
	{
		TrainingParticle active = getActive();
		if (active == null)
			return;
		active.setPosition(x, y);
		if (getFrame().getParticlesJDialog() != null)
			active.getParticleCanvas(getFrame()).repaint();
	}

	private void runXmippProgram(String program, String args)
	{
		try
		{
			Program.runByName(program, args);
		}
		catch (Exception e)
		{
			TrainingPicker.getLogger().log(Level.SEVERE, e.getMessage(), e);
			throw new IllegalArgumentException(e);
		}
	}

	

}

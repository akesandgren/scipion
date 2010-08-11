/***************************************************************************
 *
 * Authors:     Enrique Recarte Llorens   (erecallo@hotmail.com)
 *              Carlos Oscar S. Sorzano   (coss@cnb.csic.es)
 *
 * Unidad de  Bioinformatica of Centro Nacional de Biotecnologia , CSIC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or   
 * (at your option) any later version.                                 
 *                                                                     
 * This program is distributed in the hope that it will be useful,     
 * but WITHOUT ANY WARRANTY; without even the implied warranty of      
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       
 * GNU General Public License for more details.                        
 *                                                                     
 * You should have received a copy of the GNU General Public License   
 * along with this program; if not, write to the Free Software         
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA            
 * 02111-1307  USA                                                     
 *                                                                     
 *  All comments concerning this program package may be sent to the    
 *  e-mail address 'xmipp@cnb.csic.es'                                  
 ***************************************************************************/

#ifndef XMIPP__NAIVE_BAYES_HH__
#define XMIPP__NAIVE_BAYES_HH__

/* Includes ---------------------------------------------------------------- */
#include <vector>
#include <cmath>

#include <data/matrix2d.h>
#include <data/matrix1d.h>
#include <data/histogram.h>

/**@defgroup NaiveBayesClassifier Naive Bayes Classifier
   @ingroup ClassificationLibrary */
//@{

/** Leaf node class.
 * @ingroup NaiveBayesClassifier
 *  This class is for a single feature. After receiving a set
 *  of features, it discretizes the features and builds
 *  the probability density function in an irregular histogram.
 *  There is an irregular histogram for each class.
 */
class LeafNode
{
public:
    // The number of levels used for the discretization
    int __discreteLevels; 

    // PDFs for each class
    std::vector < IrregularHistogram1D > __leafPDF;
    
    // Number of classes
    int K;
public:			
    /// Constructor
    LeafNode(const std::vector < MultidimArray<double> > &leafFeatures,
        int _discrete_levels=8);

    /// Get number of levels
    int getNumberOfLevels();

    /// Assign probability to a value within the PDF of a given class
    double assignProbability(double value, int k);
    
    /// Compute weight of this leaf as a classifier
    double computeWeight() const;
    
    /// Show
    friend std::ostream & operator << (std::ostream &_out,
        const LeafNode &leaf);
};

/** Naive Bayes classifier class.
 * @ingroup NaiveBayesClassifier
 */
class NaiveBayes
{
public:
    // Number of classes
    int K;

    // Number of features
    int Nfeatures;

    // Prior probabilities of the classes
    MultidimArray<double> __priorProbsLog10;

    // Weight of each feature
    MultidimArray<double>        __weights;

    // The vector containing the Leafs (as many leafs as features)   
    std::vector<LeafNode*>  __leafs;
    
    // Cost matrix
    // C(i,j) is the cost of predicting class j when the true
    // class is class i.
    MultidimArray<double> __cost;
public:	
    // Constructor
    NaiveBayes(
        const std::vector < MultidimArray<double> >  &features,
        const MultidimArray<double> &priorProbs,
        int discreteLevels);

    // Destructor
    ~NaiveBayes();

    // Set cost matrix
    void setCostMatrix(const MultidimArray<double> &cost);

    // Returns the class with the largest probability given a set of features
    int doInference(const MultidimArray<double> &newFeatures, double &cost);
    
    /// Show
    friend std::ostream & operator << (std::ostream &_out,
        const NaiveBayes &naive);
};

/** Ensemble NaiveBayes classifier.
 * @ingroup NaiveBayesClassifier
 */
class EnsembleNaiveBayes
{
public:
    // Ensemble of classifiers
    std::vector< NaiveBayes * > ensemble;
    
    // Ensemble of features for each classifier
    std::vector< MultidimArray<int> > ensembleFeatures;
    
    // Number of classes
    int K;
    
    // Judge combination
    std::string judgeCombination;
public:
    // Constructor
    EnsembleNaiveBayes(
        const std::vector < MultidimArray<double> >  &features,
        const MultidimArray<double> &priorProbs,
        int discreteLevels, int numberOfClassifiers,
        double samplingFeatures, double samplingIndividuals,
        const std::string &newJudgeCombination);

    // Destructor
    ~EnsembleNaiveBayes();

    // Set cost matrix
    void setCostMatrix(const MultidimArray<double> &cost);

    // Returns the class with the largest probability given a set of features
    int doInference(const MultidimArray<double> &newFeatures, double &cost,
        MultidimArray<int> &votes);
};
//@}
#endif

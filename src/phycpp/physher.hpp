// Copyright 2016-2022 Mathieu Fourment.
// physher is free software under the GPLv2; see LICENSE file for details.

#pragma once

#include <stddef.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "phyc/demographicmodels.h"
#include "phyc/gradient.h"
#include "phyc/treelikelihood.h"
}

enum class GradientFlags {
    TREE_RATIO = GRADIENT_FLAG_TREE_RATIOS,
    TREE_HEIGHT = GRADIENT_FLAG_TREE_HEIGHTS,
    COALESCENT_THETA = GRADIENT_FLAG_COALESCENT_THETA
};

enum class TreeLikelihoodGradientFlags {
    TREE_HEIGHT = TREELIKELIHOOD_FLAG_TREE,
    SITE_MODEL = TREELIKELIHOOD_FLAG_SITE_MODEL,
    SUBSTITUTION_MODEL = TREELIKELIHOOD_FLAG_SUBSTITUTION_MODEL,
    BRANCH_MODEL = TREELIKELIHOOD_FLAG_BRANCH_MODEL
};

class ModelInterface {
   public:
    virtual ~ModelInterface() { model_->free(model_); }

    virtual void SetParameters(const double *parameters) = 0;
    virtual void GetParameters(double *parameters) = 0;

    Model *GetModel() { return model_; }

    void *GetManagedObject() { return model_->obj; }

    size_t parameterCount_;

   protected:
    Model *model_;
};

class CallableModelInterface : public ModelInterface {
   public:
    double LogLikelihood() { return model_->logP(model_); }

    virtual void Gradient(double *gradient) = 0;

    size_t gradientLength_;
};

class TreeModelInterface : public ModelInterface {
   public:
    TreeModelInterface(const std::string &newick, const std::vector<std::string> &taxa,
                       std::optional<const std::vector<double>> dates);

    size_t GetNodeCount() { return nodeCount_; }

    size_t GetTipCount() { return tipCount_; }

    Tree *GetTree() { return tree_; }

   protected:
    void InitializeMap(const std::vector<std::string> &taxa);

   public:
    std::vector<size_t> nodeMap_;

   protected:
    Tree *tree_;
    size_t nodeCount_;
    size_t tipCount_;
};

class UnRootedTreeModelInterface : public TreeModelInterface {
   public:
    UnRootedTreeModelInterface(const std::string &newick,
                               const std::vector<std::string> &taxa);

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override {}
};

class ReparameterizedTimeTreeModelInterface : public TreeModelInterface {
   public:
    ReparameterizedTimeTreeModelInterface(const std::string &newick,
                                          const std::vector<std::string> &taxa,
                                          const std::vector<double> dates);

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override {}

    void GetNodeHeights(double *heights);

    void GradientTransformJVP(double *gradient, const double *height_gradient);

    void GradientTransformJVP(double *gradient, const double *height_gradient,
                              const double *heights);

    void GradientTransformJacobian(double *gradient);

    double TransformJacobian();

   protected:
    Model *transformModel_;
};

class BranchModelInterface : public ModelInterface {
   public:
    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override;

    void SetRates(const double *rates);

   protected:
    BranchModel *branchModel_;
};

class StrictClockModelInterface : public BranchModelInterface {
   public:
    StrictClockModelInterface(double rate, TreeModelInterface *treeModel);

    void SetRate(double rate);
};

class SimpleClockModelInterface : public BranchModelInterface {
   public:
    SimpleClockModelInterface(const std::vector<double> &rates,
                              TreeModelInterface *treeModel);
};

class SubstitutionModelInterface : public ModelInterface {
   protected:
    Model *Initialize(const std::string &name, Parameters *rates, Model *frequencies,
                      Model *rates_model);

    SubstitutionModel *substModel_;
};

class JC69Interface : public SubstitutionModelInterface {
   public:
    JC69Interface();

    void SetParameters(const double *parameters) override {}

    void GetParameters(double *parameters) override {}
};

class HKYInterface : public SubstitutionModelInterface {
   public:
    HKYInterface(double kappa, const std::vector<double> &frequencies);

    void SetKappa(double kappa);

    void SetFrequencies(const double *frequencies);

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override {}
};

class GTRInterface : public SubstitutionModelInterface {
   public:
    GTRInterface(const std::vector<double> &rates,
                 const std::vector<double> &frequencies);

    void SetRates(const double *rates);

    void SetFrequencies(const double *frequencies);

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override {}
};

class SiteModelInterface : public ModelInterface {
   public:
    void SetMu(double mu);

   protected:
    SiteModel *siteModel_;
};

class ConstantSiteModelInterface : public SiteModelInterface {
   public:
    explicit ConstantSiteModelInterface(std::optional<double> mu);

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override;
};

class InvariantSiteModelInterface : public SiteModelInterface {
   public:
    InvariantSiteModelInterface(double proportionInvariant, std::optional<double> mu);

    void SetProportionInvariant(double value);

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override;
};

class DiscretizedSiteModelInterface : public SiteModelInterface {
   public:
    DiscretizedSiteModelInterface(distribution_t distribution, double shape,
                                  size_t categories,
                                  std::optional<double> proportionInvariant,
                                  std::optional<double> mu);

    void SetParameter(double parameter);

    void SetProportionInvariant(double value);

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override;
};

class WeibullSiteModelInterface : public DiscretizedSiteModelInterface {
   public:
    WeibullSiteModelInterface(double shape, size_t categories,
                              std::optional<double> proportionInvariant,
                              std::optional<double> mu)
        : DiscretizedSiteModelInterface(DISTRIBUTION_WEIBULL, shape, categories,
                                        proportionInvariant, mu) {}

    void SetShape(double shape);
};

class GammaSiteModelInterface : public DiscretizedSiteModelInterface {
   public:
    GammaSiteModelInterface(double shape, size_t categories,
                            std::optional<double> proportionInvariant,
                            std::optional<double> mu)
        : DiscretizedSiteModelInterface(DISTRIBUTION_GAMMA, shape, categories,
                                        proportionInvariant, mu) {
        siteModel_->epsilon = 1.e-6;
    }

    void SetShape(double shape);

    void SetEpsilon(double epsilon);
};

class TreeLikelihoodInterface : public CallableModelInterface {
   public:
    TreeLikelihoodInterface(
        const std::vector<std::pair<std::string, std::string>> &alignment,
        TreeModelInterface *treeModel, SubstitutionModelInterface *substitutionModel,
        SiteModelInterface *siteModel,
        std::optional<BranchModelInterface *> branchModel, bool use_ambiguities = false,
        bool use_tip_states = false, bool include_jacobian = false);

    void RequestGradient(std::vector<TreeLikelihoodGradientFlags> flags =
                             std::vector<TreeLikelihoodGradientFlags>());

    void Gradient(double *gradient) override;

    void SetParameters(const double *parameters) override{};

    void GetParameters(double *parameters) override{};

   private:
    TreeModelInterface *treeModel_;
    SubstitutionModelInterface *substitutionModel_;
    SiteModelInterface *siteModel_;
    BranchModelInterface *branchModel_;
    SitePattern *sitePattern_;
};

class CTMCScaleModelInterface : public CallableModelInterface {
   public:
    CTMCScaleModelInterface(const std::vector<double> rates,
                            TreeModelInterface *treeModel);

    void RequestGradient(
        std::vector<GradientFlags> flags = std::vector<GradientFlags>());

    void Gradient(double *gradient) override;

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override {}

   protected:
    DistributionModel *ctmcScale_;

   private:
    TreeModelInterface *treeModel_;
};

class CoalescentModelInterface : public CallableModelInterface {
   protected:
    explicit CoalescentModelInterface(TreeModelInterface *treeModel)
        : treeModel_(treeModel) {}

   public:
    void RequestGradient(
        std::vector<GradientFlags> flags = std::vector<GradientFlags>());

    void Gradient(double *gradient) override;

    void SetParameters(const double *parameters) override;

    void GetParameters(double *parameters) override;

   protected:
    Coalescent *coalescent_;

   private:
    TreeModelInterface *treeModel_;
};

class ConstantCoalescentModelInterface : public CoalescentModelInterface {
   public:
    ConstantCoalescentModelInterface(double theta, TreeModelInterface *treeModel);
};

class PiecewiseConstantCoalescentInterface : public CoalescentModelInterface {
   public:
    PiecewiseConstantCoalescentInterface(const std::vector<double> &thetas,
                                         TreeModelInterface *treeModel);
};

class PiecewiseConstantCoalescentGridInterface : public CoalescentModelInterface {
   public:
    PiecewiseConstantCoalescentGridInterface(const std::vector<double> &thetas,
                                             TreeModelInterface *treeModel,
                                             double cutoff);
};

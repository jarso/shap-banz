#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <Python.h>
#include <numpy/arrayobject.h>
#include "tree_shap.h"
#include <iostream>
#include <stack>
#include <set>

#define COUT(x) //std::cout << x << std::endl;
#define SCOUT(s,x) //std::cout << s << " " << x << std::endl;
#define TAIL parent, tree, features_count, feature_results, betas, deltas, deltas_star, B, S, F, H, tree.node_sample_weights

static PyObject *_cext_dense_tree_shap(PyObject *self, PyObject *args);
static PyObject *_cext_dense_tree_banz(PyObject *self, PyObject *args);
static PyObject *_cext_dense_tree_predict(PyObject *self, PyObject *args);
static PyObject *_cext_dense_tree_update_weights(PyObject *self, PyObject *args);
static PyObject *_cext_dense_tree_saabas(PyObject *self, PyObject *args);
static PyObject *_cext_compute_expectations(PyObject *self, PyObject *args);

static PyMethodDef module_methods[] = {
    {"dense_tree_shap", _cext_dense_tree_shap, METH_VARARGS, "C implementation of Tree SHAP for dense."},
    {"dense_tree_banz", _cext_dense_tree_banz, METH_VARARGS, "C implementation of Banzhaf for dense."},
    {"dense_tree_predict", _cext_dense_tree_predict, METH_VARARGS, "C implementation of tree predictions."},
    {"dense_tree_update_weights", _cext_dense_tree_update_weights, METH_VARARGS, "C implementation of tree node weight compuatations."},
    {"dense_tree_saabas", _cext_dense_tree_saabas, METH_VARARGS, "C implementation of Saabas (rough fast approximation to Tree SHAP)."},
    {"compute_expectations", _cext_compute_expectations, METH_VARARGS, "Compute expectations of internal nodes."},
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_cext",
    "This module provides an interface for a fast Tree SHAP implementation.",
    -1,
    module_methods,
    NULL,
    NULL,
    NULL,
    NULL
};
#endif

#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC PyInit__cext(void)
#else
PyMODINIT_FUNC init_cext(void)
#endif
{
    #if PY_MAJOR_VERSION >= 3
        PyObject *module = PyModule_Create(&moduledef);
        if (!module) return NULL;
    #else
        PyObject *module = Py_InitModule("_cext", module_methods);
        if (!module) return;
    #endif

    /* Load `numpy` functionality. */
    import_array();

    #if PY_MAJOR_VERSION >= 3
        return module;
    #endif
}

static PyObject *_cext_compute_expectations(PyObject *self, PyObject *args)
{
    PyObject *children_left_obj;
    PyObject *children_right_obj;
    PyObject *node_sample_weight_obj;
    PyObject *values_obj;
    
    /* Parse the input tuple */
    if (!PyArg_ParseTuple(
        args, "OOOO", &children_left_obj, &children_right_obj, &node_sample_weight_obj, &values_obj
    )) return NULL;

    /* Interpret the input objects as numpy arrays. */
    PyArrayObject *children_left_array = (PyArrayObject*)PyArray_FROM_OTF(children_left_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_right_array = (PyArrayObject*)PyArray_FROM_OTF(children_right_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *node_sample_weight_array = (PyArrayObject*)PyArray_FROM_OTF(node_sample_weight_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *values_array = (PyArrayObject*)PyArray_FROM_OTF(values_obj, NPY_DOUBLE, NPY_ARRAY_INOUT_ARRAY);

    /* If that didn't work, throw an exception. */
    if (children_left_array == NULL || children_right_array == NULL ||
        values_array == NULL || node_sample_weight_array == NULL) {
        Py_XDECREF(children_left_array);
        Py_XDECREF(children_right_array);
        //PyArray_ResolveWritebackIfCopy(values_array);
        Py_XDECREF(values_array);
        Py_XDECREF(node_sample_weight_array);
        return NULL;
    }

    TreeEnsemble tree;

    // number of outputs
    tree.num_outputs = PyArray_DIM(values_array, 1);

    /* Get pointers to the data as C-types. */
    tree.children_left = (int*)PyArray_DATA(children_left_array);
    tree.children_right = (int*)PyArray_DATA(children_right_array);
    tree.values = (tfloat*)PyArray_DATA(values_array);
    tree.node_sample_weights = (tfloat*)PyArray_DATA(node_sample_weight_array);

    const int max_depth = compute_expectations(tree);

    // clean up the created python objects
    Py_XDECREF(children_left_array);
    Py_XDECREF(children_right_array);
    //PyArray_ResolveWritebackIfCopy(values_array);
    Py_XDECREF(values_array);
    Py_XDECREF(node_sample_weight_array);

    PyObject *ret = Py_BuildValue("i", max_depth);
    return ret;
}


static PyObject *_cext_dense_tree_shap(PyObject *self, PyObject *args)
{
    COUT("dense tree shap")
    PyObject *children_left_obj;
    PyObject *children_right_obj;
    PyObject *children_default_obj;
    PyObject *features_obj;
    PyObject *thresholds_obj;
    PyObject *values_obj;
    PyObject *node_sample_weights_obj;
    int max_depth;
    PyObject *X_obj;
    PyObject *X_missing_obj;
    PyObject *y_obj;
    PyObject *R_obj;
    PyObject *R_missing_obj;
    int tree_limit;
    PyObject *out_contribs_obj;
    int feature_dependence;
    int model_output;
    PyObject *base_offset_obj;
    bool interactions;
  
    /* Parse the input tuple */
    if (!PyArg_ParseTuple(
        args, "OOOOOOOiOOOOOiOOiib", &children_left_obj, &children_right_obj, &children_default_obj,
        &features_obj, &thresholds_obj, &values_obj, &node_sample_weights_obj,
        &max_depth, &X_obj, &X_missing_obj, &y_obj, &R_obj, &R_missing_obj, &tree_limit, &base_offset_obj,
        &out_contribs_obj, &feature_dependence, &model_output, &interactions
    )) return NULL;

    /* Interpret the input objects as numpy arrays. */
    PyArrayObject *children_left_array = (PyArrayObject*)PyArray_FROM_OTF(children_left_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_right_array = (PyArrayObject*)PyArray_FROM_OTF(children_right_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_default_array = (PyArrayObject*)PyArray_FROM_OTF(children_default_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *features_array = (PyArrayObject*)PyArray_FROM_OTF(features_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *thresholds_array = (PyArrayObject*)PyArray_FROM_OTF(thresholds_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *values_array = (PyArrayObject*)PyArray_FROM_OTF(values_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *node_sample_weights_array = (PyArrayObject*)PyArray_FROM_OTF(node_sample_weights_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_array = (PyArrayObject*)PyArray_FROM_OTF(X_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_missing_array = (PyArrayObject*)PyArray_FROM_OTF(X_missing_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *y_array = NULL;
    if (y_obj != Py_None) y_array = (PyArrayObject*)PyArray_FROM_OTF(y_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *R_array = NULL;
    if (R_obj != Py_None) R_array = (PyArrayObject*)PyArray_FROM_OTF(R_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *R_missing_array = NULL;
    if (R_missing_obj != Py_None) R_missing_array = (PyArrayObject*)PyArray_FROM_OTF(R_missing_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *out_contribs_array = (PyArrayObject*)PyArray_FROM_OTF(out_contribs_obj, NPY_DOUBLE, NPY_ARRAY_INOUT_ARRAY);
    PyArrayObject *base_offset_array = (PyArrayObject*)PyArray_FROM_OTF(base_offset_obj, NPY_DOUBLE, NPY_ARRAY_INOUT_ARRAY);

    /* If that didn't work, throw an exception. Note that R and y are optional. */
    if (children_left_array == NULL || children_right_array == NULL ||
        children_default_array == NULL || features_array == NULL || thresholds_array == NULL ||
        values_array == NULL || node_sample_weights_array == NULL || X_array == NULL ||
        X_missing_array == NULL || out_contribs_array == NULL) {
        Py_XDECREF(children_left_array);
        Py_XDECREF(children_right_array);
        Py_XDECREF(children_default_array);
        Py_XDECREF(features_array);
        Py_XDECREF(thresholds_array);
        Py_XDECREF(values_array);
        Py_XDECREF(node_sample_weights_array);
        Py_XDECREF(X_array);
        Py_XDECREF(X_missing_array);
        if (y_array != NULL) Py_XDECREF(y_array);
        if (R_array != NULL) Py_XDECREF(R_array);
        if (R_missing_array != NULL) Py_XDECREF(R_missing_array);
        //PyArray_ResolveWritebackIfCopy(out_contribs_array);
        Py_XDECREF(out_contribs_array);
        Py_XDECREF(base_offset_array);
        return NULL;
    }

    const unsigned num_X = PyArray_DIM(X_array, 0);
    const unsigned M = PyArray_DIM(X_array, 1);
    const unsigned max_nodes = PyArray_DIM(values_array, 1);
    const unsigned num_outputs = PyArray_DIM(values_array, 2);
    unsigned num_R = 0;
    if (R_array != NULL) num_R = PyArray_DIM(R_array, 0);

    // Get pointers to the data as C-types
    int *children_left = (int*)PyArray_DATA(children_left_array);
    int *children_right = (int*)PyArray_DATA(children_right_array);
    int *children_default = (int*)PyArray_DATA(children_default_array);
    int *features = (int*)PyArray_DATA(features_array);
    tfloat *thresholds = (tfloat*)PyArray_DATA(thresholds_array);
    tfloat *values = (tfloat*)PyArray_DATA(values_array);
    tfloat *node_sample_weights = (tfloat*)PyArray_DATA(node_sample_weights_array);
    tfloat *X = (tfloat*)PyArray_DATA(X_array);
    bool *X_missing = (bool*)PyArray_DATA(X_missing_array);
    tfloat *y = NULL;
    if (y_array != NULL) y = (tfloat*)PyArray_DATA(y_array);
    tfloat *R = NULL;
    if (R_array != NULL) R = (tfloat*)PyArray_DATA(R_array);
    bool *R_missing = NULL;
    if (R_missing_array != NULL) R_missing = (bool*)PyArray_DATA(R_missing_array);
    tfloat *out_contribs = (tfloat*)PyArray_DATA(out_contribs_array);
    tfloat *base_offset = (tfloat*)PyArray_DATA(base_offset_array);

    // these are just a wrapper objects for all the pointers and numbers associated with
    // the ensemble tree model and the datset we are explaing
    TreeEnsemble trees = TreeEnsemble(
        children_left, children_right, children_default, features, thresholds, values,
        node_sample_weights, max_depth, tree_limit, base_offset,
        max_nodes, num_outputs
    );
    ExplanationDataset data = ExplanationDataset(X, X_missing, y, R, R_missing, num_X, M, num_R);

    dense_tree_shap(trees, data, out_contribs, feature_dependence, model_output, interactions);

    // retrieve return value before python cleanup of objects
    tfloat ret_value = (double)values[0];

    // clean up the created python objects 
    Py_XDECREF(children_left_array);
    Py_XDECREF(children_right_array);
    Py_XDECREF(children_default_array);
    Py_XDECREF(features_array);
    Py_XDECREF(thresholds_array);
    Py_XDECREF(values_array);
    Py_XDECREF(node_sample_weights_array);
    Py_XDECREF(X_array);
    Py_XDECREF(X_missing_array);
    if (y_array != NULL) Py_XDECREF(y_array);
    if (R_array != NULL) Py_XDECREF(R_array);
    if (R_missing_array != NULL) Py_XDECREF(R_missing_array);
    //PyArray_ResolveWritebackIfCopy(out_contribs_array);
    Py_XDECREF(out_contribs_array);
    Py_XDECREF(base_offset_array);

    /* Build the output tuple */
    PyObject *ret = Py_BuildValue("d", ret_value);
    return ret;
}

static void set_parent(int *parent, int size, TreeEnsemble &tree)
{
    for (size_t i = 0; i < size; i++)
    {
        if (tree.children_right[i] != -1)
        {
            parent[tree.children_right[i]] = i;
        }
        
        if (tree.children_left[i] != -1)
        {
            parent[tree.children_left[i]] = i;
        }
    }
}


inline void proper_tree_banz(const TreeEnsemble& trees, const ExplanationDataset &data, tfloat *out_contribs) {
    return;
}


inline void fast(int node, int* parent_list, TreeEnsemble& tree, int features_count, double* feature_results, double* betas, 
    double* deltas, double* deltas_star, double* B, double* S, std::set<int>* F, std::stack<int>** H, double* r) {

    if (node == -1) {
        return;
    }

    int parent = parent_list[node];
    int* features = tree.features;
    H[features[parent]]->push(node);

    if ((tree.children_left[node] == -1) && (tree.children_right[node] == -1)) {
        SCOUT("lisciem jest",node)
        SCOUT("betas",betas[node])
        SCOUT("values",tree.values[node])
        S[node] = betas[node] * tree.values[node];
        SCOUT("S[node] =",S[node])
    } else {
        fast(tree.children_left[node],
            parent_list, tree, features_count, feature_results, betas, deltas, deltas_star, B, S, F, H, r);

        fast(tree.children_right[node],
            parent_list, tree, features_count, feature_results, betas, deltas, deltas_star, B, S, F, H, r);

        int left_child = tree.children_left[node];
        int right_child = tree.children_right[node];
        SCOUT("S[",node)
        SCOUT("lewy:",left_child)
        SCOUT("prawy:",right_child)
        SCOUT("S[lewy]:",S[left_child])
        SCOUT("S[prawy]:",S[right_child])

        S[node] += (left_child == -1 ? 0 : S[left_child]);
        S[node] += (right_child == -1 ? 0 : S[right_child]);

        SCOUT("nowy S[node]",S[node])
    }

    int z = 0;
    while (H[features[parent]]->top() != node) {
        z += S[H[features[parent]]->top()];
        H[features[parent]]->pop();
    }

    B[node] = S[node] - z;

    if (H[features[parent]]->size() == 1) {
        H[features[parent]]->pop();
    }

    return;
}

inline void traverse(int node, const tfloat *x, int* parent_list, TreeEnsemble& tree, int features_count, double* feature_results, double* betas,
    double* deltas, double* deltas_star, double* B, double* S, std::set<int>* F, std::stack<int>** H, double* r) {

    bool present;
    double b, delta_old;
    int* features = tree.features;
    int parent = parent_list[node];
    if (node == -1)
        return;
    
    if (F->find(features[parent]) != F->end()) { //d_p_v in F
        present = true;
        b = 2 / (1 + deltas[features[parent]]) * betas[tree.features[node]];
    }
    else {
        present = false;
        F->insert(features[parent]);
        b = betas[parent];
    }
    delta_old = deltas[features[parent]];
    deltas[features[parent]] = deltas[features[parent]] * (r[parent] / r[node]);

    if (node == tree.children_left[parent]) {
        deltas[features[parent]] = deltas[features[parent]] * double(x[features[parent]] < tree.thresholds[parent]);
    }
    else {
        deltas[features[parent]] = deltas[features[parent]] * double(x[features[parent]] >= tree.thresholds[parent]);
    }
    deltas_star[node] = deltas[features[parent]];

    b = b * (r[node] / r[parent]);
    betas[node] = b * (1 + deltas[features[parent]]) / 2;

//    std::cout << "dla v = " << node << ", betas[v] = " << betas[node] << std::endl;
    COUT("parent")
    COUT(parent)
    COUT("features")
    COUT(features[parent])
    COUT("deltas")
    COUT(deltas[features[parent]])
    COUT("x[y]")
    COUT(x[features[parent]])
    COUT("czy?")
    COUT((x[features[parent]] < tree.thresholds[parent]))

    

    traverse(tree.children_left[node], x, parent_list, tree, features_count, feature_results, betas,
        deltas, deltas_star, B, S, F, H, r);

    traverse(tree.children_right[node], x, parent_list, tree, features_count, feature_results, betas,
        deltas, deltas_star, B, S, F, H, r);

    if (!present) {
        F->erase(features[parent]);
    }

    deltas[features[parent]] = delta_old;

    return;
}

inline void dense_tree_banz(const TreeEnsemble& trees, const ExplanationDataset &data, tfloat *out_contribs,
                     const int feature_dependence, unsigned model_transform, bool interactions) {
    std::cout << "zaczynamy!" << std::endl;
    // initializing values
    // TODO why double and not float / tfloat ???
    int features_count = trees.max_nodes;
    double* feature_results = new double[features_count]; // todo liczba cech
    double* betas = new double[trees.max_nodes];
    double* deltas = new double[features_count]; // todo liczba cech
    double* deltas_star = new double[trees.max_nodes];
    double* B = new double[trees.max_nodes];
    double* S = new double[trees.max_nodes];

    std::set<int>* F = new std::set<int>;

    std::stack<int>** H = new std::stack<int>*[features_count]; // todo liczba cech

    for (unsigned i = 0; i < features_count; ++i) {
        H[i] = new std::stack<int>;
        deltas[i] = 1;
        feature_results[i] = 0;
    }
    
    for (unsigned i = 0; i < trees.max_nodes; ++i) {
        deltas_star[i] = 0;
        betas[i] = 1;
        B[i] = 0;
        S[i] = 0;
        feature_results[i] = 0;
    }

    // proper calculations
    for (unsigned i = 0; i < trees.tree_limit; ++i) {
        SCOUT("drzewo nr",i)
        COUT("ile drzew?")
        COUT(trees.tree_limit)
        TreeEnsemble tree;
        trees.get_tree(tree, i);
        int* parent = new int[features_count];
        set_parent(parent, trees.max_nodes, tree);

        // std::cout<<"parent:\n";
        // for (int ii = 0; ii < trees.max_nodes; ii++)
        //     std::cout << parent[ii] << std::endl;

        unsigned root = 0; // ?? czy na pewno?

        traverse(tree.children_left[root], data.X, TAIL);
//        fast(tree.children_left[root], TAIL);
        traverse(tree.children_right[root], data.X, TAIL);
//        fast(tree.children_right[root], TAIL);

        COUT("rozmiary:")
        COUT(data.M) // !! rozmiar probki
        COUT(data.num_X) // !! liczba probek
        COUT(trees.max_nodes) // !! liczba max wierzcholkow
        COUT("koniec rozmiarow")

//        std::cout << "betas for :" << "\n";
//        for (unsigned i = 0; i < trees.max_nodes; ++i)
//            std::cout << betas[i] << std::endl;
//
//        std::cout << "probka: [ " << data.X[0] << ", " << data.X[1] << ", ... ]" << std::endl;
//        for (int ii = 0; ii < trees.max_nodes; ii++)
//            std::cout << ii << " :: " << betas[ii] << std::endl;

        int number_of_nodes = trees.max_nodes;
        for (unsigned v = 1; v < number_of_nodes; ++v) {
//            std::cout << "dla node: " << v << " i feature: " << tree.features[parent[v]] << std::endl;
            COUT(B[v])
            COUT(deltas_star[v])
            feature_results[tree.features[parent[v]]] += 2 * B[v] * (deltas_star[v] - 1) / (1 + deltas_star[v]);
        }
        delete[] parent;
    }

    for (unsigned i = 0; i < data.M; ++i) {
        out_contribs[i] = feature_results[i];
    }

    return;
}

static PyObject *_cext_dense_tree_banz(PyObject *self, PyObject *args)
{
    COUT("dense tree banz")
    COUT(args)
    PyObject *children_left_obj;
    PyObject *children_right_obj;
    PyObject *children_default_obj;
    PyObject *features_obj;
    PyObject *thresholds_obj;
    PyObject *values_obj;
    PyObject *node_sample_weights_obj;
    int max_depth;
    PyObject *X_obj;
    PyObject *X_missing_obj;
    PyObject *y_obj;
    PyObject *R_obj;
    PyObject *R_missing_obj;
    int tree_limit;
    PyObject *out_contribs_obj; // tu jest output
    int feature_dependence;
    int model_output;
    PyObject *base_offset_obj;
    bool interactions;

    /* Parse the input tuple */
    if (!PyArg_ParseTuple(
        args, "OOOOOOOiOOOOOiOOiib", &children_left_obj, &children_right_obj, &children_default_obj,
        &features_obj, &thresholds_obj, &values_obj, &node_sample_weights_obj,
        &max_depth, &X_obj, &X_missing_obj, &y_obj, &R_obj, &R_missing_obj, &tree_limit, &base_offset_obj,
        &out_contribs_obj, &feature_dependence, &model_output, &interactions
    )) return NULL;

    /* Interpret the input objects as numpy arrays. */
    PyArrayObject *children_left_array = (PyArrayObject*)PyArray_FROM_OTF(children_left_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_right_array = (PyArrayObject*)PyArray_FROM_OTF(children_right_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_default_array = (PyArrayObject*)PyArray_FROM_OTF(children_default_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *features_array = (PyArrayObject*)PyArray_FROM_OTF(features_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *thresholds_array = (PyArrayObject*)PyArray_FROM_OTF(thresholds_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *values_array = (PyArrayObject*)PyArray_FROM_OTF(values_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *node_sample_weights_array = (PyArrayObject*)PyArray_FROM_OTF(node_sample_weights_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_array = (PyArrayObject*)PyArray_FROM_OTF(X_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_missing_array = (PyArrayObject*)PyArray_FROM_OTF(X_missing_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *y_array = NULL;
    if (y_obj != Py_None) y_array = (PyArrayObject*)PyArray_FROM_OTF(y_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *R_array = NULL;
    if (R_obj != Py_None) R_array = (PyArrayObject*)PyArray_FROM_OTF(R_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *R_missing_array = NULL;
    if (R_missing_obj != Py_None) R_missing_array = (PyArrayObject*)PyArray_FROM_OTF(R_missing_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *out_contribs_array = (PyArrayObject*)PyArray_FROM_OTF(out_contribs_obj, NPY_DOUBLE, NPY_ARRAY_INOUT_ARRAY);
    PyArrayObject *base_offset_array = (PyArrayObject*)PyArray_FROM_OTF(base_offset_obj, NPY_DOUBLE, NPY_ARRAY_INOUT_ARRAY);

    /* If that didn't work, throw an exception. Note that R and y are optional. */
    if (children_left_array == NULL || children_right_array == NULL ||
        children_default_array == NULL || features_array == NULL || thresholds_array == NULL ||
        values_array == NULL || node_sample_weights_array == NULL || X_array == NULL ||
        X_missing_array == NULL || out_contribs_array == NULL) {
        Py_XDECREF(children_left_array);
        Py_XDECREF(children_right_array);
        Py_XDECREF(children_default_array);
        Py_XDECREF(features_array);
        Py_XDECREF(thresholds_array);
        Py_XDECREF(values_array);
        Py_XDECREF(node_sample_weights_array);
        Py_XDECREF(X_array);
        Py_XDECREF(X_missing_array);
        if (y_array != NULL) Py_XDECREF(y_array);
        if (R_array != NULL) Py_XDECREF(R_array);
        if (R_missing_array != NULL) Py_XDECREF(R_missing_array);
        //PyArray_ResolveWritebackIfCopy(out_contribs_array);
        Py_XDECREF(out_contribs_array);
        Py_XDECREF(base_offset_array);
        return NULL;
    }

    const unsigned num_X = PyArray_DIM(X_array, 0);
    const unsigned M = PyArray_DIM(X_array, 1);
    const unsigned max_nodes = PyArray_DIM(values_array, 1);
    const unsigned num_outputs = PyArray_DIM(values_array, 2);
    unsigned num_R = 0;
    if (R_array != NULL) num_R = PyArray_DIM(R_array, 0);

    // Get pointers to the data as C-types
    int *children_left = (int*)PyArray_DATA(children_left_array);
    int *children_right = (int*)PyArray_DATA(children_right_array);
    int *children_default = (int*)PyArray_DATA(children_default_array);
    int *features = (int*)PyArray_DATA(features_array);
    tfloat *thresholds = (tfloat*)PyArray_DATA(thresholds_array);
    tfloat *values = (tfloat*)PyArray_DATA(values_array);
    tfloat *node_sample_weights = (tfloat*)PyArray_DATA(node_sample_weights_array);
    tfloat *X = (tfloat*)PyArray_DATA(X_array);
    bool *X_missing = (bool*)PyArray_DATA(X_missing_array);
    tfloat *y = NULL;
    if (y_array != NULL) y = (tfloat*)PyArray_DATA(y_array);
    tfloat *R = NULL;
    if (R_array != NULL) R = (tfloat*)PyArray_DATA(R_array);
    bool *R_missing = NULL;
    if (R_missing_array != NULL) R_missing = (bool*)PyArray_DATA(R_missing_array);
    tfloat *out_contribs = (tfloat*)PyArray_DATA(out_contribs_array);
    tfloat *base_offset = (tfloat*)PyArray_DATA(base_offset_array);

    // these are just a wrapper objects for all the pointers and numbers associated with
    // the ensemble tree model and the datset we are explaing
    TreeEnsemble trees = TreeEnsemble(
        children_left, children_right, children_default, features, thresholds, values,
        node_sample_weights, max_depth, tree_limit, base_offset,
        max_nodes, num_outputs
    );
    ExplanationDataset data = ExplanationDataset(X, X_missing, y, R, R_missing, num_X, M, num_R);

    dense_tree_banz(trees, data, out_contribs, feature_dependence, model_output, interactions);

    // retrieve return value before python cleanup of objects
    tfloat ret_value = (double)values[0];

    // clean up the created python objects
    Py_XDECREF(children_left_array);
    Py_XDECREF(children_right_array);
    Py_XDECREF(children_default_array);
    Py_XDECREF(features_array);
    Py_XDECREF(thresholds_array);
    Py_XDECREF(values_array);
    Py_XDECREF(node_sample_weights_array);
    Py_XDECREF(X_array);
    Py_XDECREF(X_missing_array);
    if (y_array != NULL) Py_XDECREF(y_array);
    if (R_array != NULL) Py_XDECREF(R_array);
    if (R_missing_array != NULL) Py_XDECREF(R_missing_array);
    //PyArray_ResolveWritebackIfCopy(out_contribs_array);
    Py_XDECREF(out_contribs_array);
    Py_XDECREF(base_offset_array);

    /* Build the output tuple */
    PyObject *ret = Py_BuildValue("d", ret_value);
    return ret;
}

void dense_tree_predict(tfloat *out, const TreeEnsemble &trees, const ExplanationDataset &data, unsigned model_transform) {
    COUT("test")
    tfloat *row_out = out;
    const tfloat *x = data.X;
    const bool *x_missing = data.X_missing;

    // see what transform (if any) we have
    transform_f transform = get_transform(model_transform);

    for (unsigned i = 0; i < data.num_X; ++i) {

        // add the base offset
        for (unsigned k = 0; k < trees.num_outputs; ++k) {
            row_out[k] += trees.base_offset[k];
        }

        // add the leaf values from each tree
        for (unsigned j = 0; j < trees.tree_limit; ++j) {
            const tfloat *leaf_value = tree_predict(j, trees, x, x_missing);

            for (unsigned k = 0; k < trees.num_outputs; ++k) {
                row_out[k] += leaf_value[k];
            }
        }

        // apply any needed transform
        if (transform != NULL) {
            const tfloat y_i = data.y == NULL ? 0 : data.y[i];
            for (unsigned k = 0; k < trees.num_outputs; ++k) {
                row_out[k] = transform(row_out[k], y_i);
            }
        }

        x += data.M;
        x_missing += data.M;
        row_out += trees.num_outputs;
    }
}


static PyObject *_cext_dense_tree_predict(PyObject *self, PyObject *args)
{
    PyObject *children_left_obj;
    PyObject *children_right_obj;
    PyObject *children_default_obj;
    PyObject *features_obj;
    PyObject *thresholds_obj;
    PyObject *values_obj;
    int max_depth;
    int tree_limit;
    PyObject *base_offset_obj;
    int model_output;
    PyObject *X_obj;
    PyObject *X_missing_obj;
    PyObject *y_obj;
    PyObject *out_pred_obj;
  
    /* Parse the input tuple */
    if (!PyArg_ParseTuple(
        args, "OOOOOOiiOiOOOO", &children_left_obj, &children_right_obj, &children_default_obj,
        &features_obj, &thresholds_obj, &values_obj, &max_depth, &tree_limit, &base_offset_obj, &model_output,
        &X_obj, &X_missing_obj, &y_obj, &out_pred_obj
    )) return NULL;

    /* Interpret the input objects as numpy arrays. */
    PyArrayObject *children_left_array = (PyArrayObject*)PyArray_FROM_OTF(children_left_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_right_array = (PyArrayObject*)PyArray_FROM_OTF(children_right_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_default_array = (PyArrayObject*)PyArray_FROM_OTF(children_default_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *features_array = (PyArrayObject*)PyArray_FROM_OTF(features_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *thresholds_array = (PyArrayObject*)PyArray_FROM_OTF(thresholds_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *values_array = (PyArrayObject*)PyArray_FROM_OTF(values_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *base_offset_array = (PyArrayObject*)PyArray_FROM_OTF(base_offset_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_array = (PyArrayObject*)PyArray_FROM_OTF(X_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_missing_array = (PyArrayObject*)PyArray_FROM_OTF(X_missing_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *y_array = NULL;
    if (y_obj != Py_None) y_array = (PyArrayObject*)PyArray_FROM_OTF(y_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *out_pred_array = (PyArrayObject*)PyArray_FROM_OTF(out_pred_obj, NPY_DOUBLE, NPY_ARRAY_INOUT_ARRAY);

    /* If that didn't work, throw an exception. Note that R and y are optional. */
    if (children_left_array == NULL || children_right_array == NULL ||
        children_default_array == NULL || features_array == NULL || thresholds_array == NULL ||
        values_array == NULL || X_array == NULL ||
        X_missing_array == NULL || out_pred_array == NULL) {
        Py_XDECREF(children_left_array);
        Py_XDECREF(children_right_array);
        Py_XDECREF(children_default_array);
        Py_XDECREF(features_array);
        Py_XDECREF(thresholds_array);
        Py_XDECREF(values_array);
        Py_XDECREF(base_offset_array);
        Py_XDECREF(X_array);
        Py_XDECREF(X_missing_array);
        if (y_array != NULL) Py_XDECREF(y_array);
        //PyArray_ResolveWritebackIfCopy(out_pred_array);
        Py_XDECREF(out_pred_array);
        return NULL;
    }

    const unsigned num_X = PyArray_DIM(X_array, 0);
    const unsigned M = PyArray_DIM(X_array, 1);
    const unsigned max_nodes = PyArray_DIM(values_array, 1);
    const unsigned num_outputs = PyArray_DIM(values_array, 2);

    const unsigned num_offsets = PyArray_DIM(base_offset_array, 0);
    if (num_offsets != num_outputs) {
        std::cerr << "The passed base_offset array does that have the same number of outputs as the values array: "
            << num_offsets << " vs. " << num_outputs << std::endl;
        return NULL;
    }

    // Get pointers to the data as C-types
    int *children_left = (int*)PyArray_DATA(children_left_array);
    int *children_right = (int*)PyArray_DATA(children_right_array);
    int *children_default = (int*)PyArray_DATA(children_default_array);
    int *features = (int*)PyArray_DATA(features_array);
    tfloat *thresholds = (tfloat*)PyArray_DATA(thresholds_array);
    tfloat *values = (tfloat*)PyArray_DATA(values_array);
    tfloat *base_offset = (tfloat*)PyArray_DATA(base_offset_array);
    tfloat *X = (tfloat*)PyArray_DATA(X_array);
    bool *X_missing = (bool*)PyArray_DATA(X_missing_array);
    tfloat *y = NULL;
    if (y_array != NULL) y = (tfloat*)PyArray_DATA(y_array);
    tfloat *out_pred = (tfloat*)PyArray_DATA(out_pred_array);

    // these are just wrapper objects for all the pointers and numbers associated with
    // the ensemble tree model and the datset we are explaing
    TreeEnsemble trees = TreeEnsemble(
        children_left, children_right, children_default, features, thresholds, values,
        NULL, max_depth, tree_limit, base_offset,
        max_nodes, num_outputs
    );
    ExplanationDataset data = ExplanationDataset(X, X_missing, y, NULL, NULL, num_X, M, 0);

    dense_tree_predict(out_pred, trees, data, model_output);

    // clean up the created python objects 
    Py_XDECREF(children_left_array);
    Py_XDECREF(children_right_array);
    Py_XDECREF(children_default_array);
    Py_XDECREF(features_array);
    Py_XDECREF(thresholds_array);
    Py_XDECREF(values_array);
    Py_XDECREF(base_offset_array);
    Py_XDECREF(X_array);
    Py_XDECREF(X_missing_array);
    if (y_array != NULL) Py_XDECREF(y_array);
    //PyArray_ResolveWritebackIfCopy(out_pred_array);
    Py_XDECREF(out_pred_array);

    /* Build the output tuple */
    PyObject *ret = Py_BuildValue("d", (double)values[0]);
    return ret;
}


static PyObject *_cext_dense_tree_update_weights(PyObject *self, PyObject *args)
{
    PyObject *children_left_obj;
    PyObject *children_right_obj;
    PyObject *children_default_obj;
    PyObject *features_obj;
    PyObject *thresholds_obj;
    PyObject *values_obj;
    int tree_limit;
    PyObject *node_sample_weight_obj;
    PyObject *X_obj;
    PyObject *X_missing_obj;
  
    /* Parse the input tuple */
    if (!PyArg_ParseTuple(
        args, "OOOOOOiOOO", &children_left_obj, &children_right_obj, &children_default_obj,
        &features_obj, &thresholds_obj, &values_obj, &tree_limit, &node_sample_weight_obj, &X_obj, &X_missing_obj
    )) return NULL;

    /* Interpret the input objects as numpy arrays. */
    PyArrayObject *children_left_array = (PyArrayObject*)PyArray_FROM_OTF(children_left_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_right_array = (PyArrayObject*)PyArray_FROM_OTF(children_right_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_default_array = (PyArrayObject*)PyArray_FROM_OTF(children_default_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *features_array = (PyArrayObject*)PyArray_FROM_OTF(features_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *thresholds_array = (PyArrayObject*)PyArray_FROM_OTF(thresholds_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *values_array = (PyArrayObject*)PyArray_FROM_OTF(values_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *node_sample_weight_array = (PyArrayObject*)PyArray_FROM_OTF(node_sample_weight_obj, NPY_DOUBLE, NPY_ARRAY_INOUT_ARRAY);
    PyArrayObject *X_array = (PyArrayObject*)PyArray_FROM_OTF(X_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_missing_array = (PyArrayObject*)PyArray_FROM_OTF(X_missing_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);

    /* If that didn't work, throw an exception. */
    if (children_left_array == NULL || children_right_array == NULL ||
        children_default_array == NULL || features_array == NULL || thresholds_array == NULL ||
        values_array == NULL || node_sample_weight_array == NULL || X_array == NULL ||
        X_missing_array == NULL) {
        Py_XDECREF(children_left_array);
        Py_XDECREF(children_right_array);
        Py_XDECREF(children_default_array);
        Py_XDECREF(features_array);
        Py_XDECREF(thresholds_array);
        Py_XDECREF(values_array);
        //PyArray_ResolveWritebackIfCopy(node_sample_weight_array);
        Py_XDECREF(node_sample_weight_array);
        Py_XDECREF(X_array);
        Py_XDECREF(X_missing_array);
        std::cerr << "Found a NULL input array in _cext_dense_tree_update_weights!\n";
        return NULL;
    }

    const unsigned num_X = PyArray_DIM(X_array, 0);
    const unsigned M = PyArray_DIM(X_array, 1);
    const unsigned max_nodes = PyArray_DIM(values_array, 1);

    // Get pointers to the data as C-types
    int *children_left = (int*)PyArray_DATA(children_left_array);
    int *children_right = (int*)PyArray_DATA(children_right_array);
    int *children_default = (int*)PyArray_DATA(children_default_array);
    int *features = (int*)PyArray_DATA(features_array);
    tfloat *thresholds = (tfloat*)PyArray_DATA(thresholds_array);
    tfloat *values = (tfloat*)PyArray_DATA(values_array);
    tfloat *node_sample_weight = (tfloat*)PyArray_DATA(node_sample_weight_array);
    tfloat *X = (tfloat*)PyArray_DATA(X_array);
    bool *X_missing = (bool*)PyArray_DATA(X_missing_array);

    // these are just wrapper objects for all the pointers and numbers associated with
    // the ensemble tree model and the datset we are explaing
    TreeEnsemble trees = TreeEnsemble(
        children_left, children_right, children_default, features, thresholds, values,
        node_sample_weight, 0, tree_limit, 0, max_nodes, 0
    );
    ExplanationDataset data = ExplanationDataset(X, X_missing, NULL, NULL, NULL, num_X, M, 0);

    dense_tree_update_weights(trees, data);

    // clean up the created python objects 
    Py_XDECREF(children_left_array);
    Py_XDECREF(children_right_array);
    Py_XDECREF(children_default_array);
    Py_XDECREF(features_array);
    Py_XDECREF(thresholds_array);
    Py_XDECREF(values_array);
    //PyArray_ResolveWritebackIfCopy(node_sample_weight_array);
    Py_XDECREF(node_sample_weight_array);
    Py_XDECREF(X_array);
    Py_XDECREF(X_missing_array);

    /* Build the output tuple */
    PyObject *ret = Py_BuildValue("d", 1);
    return ret;
}


static PyObject *_cext_dense_tree_saabas(PyObject *self, PyObject *args)
{
    PyObject *children_left_obj;
    PyObject *children_right_obj;
    PyObject *children_default_obj;
    PyObject *features_obj;
    PyObject *thresholds_obj;
    PyObject *values_obj;
    int max_depth;
    int tree_limit;
    PyObject *base_offset_obj;
    int model_output;
    PyObject *X_obj;
    PyObject *X_missing_obj;
    PyObject *y_obj;
    PyObject *out_pred_obj;
    
  
    /* Parse the input tuple */
    if (!PyArg_ParseTuple(
        args, "OOOOOOiiOiOOOO", &children_left_obj, &children_right_obj, &children_default_obj,
        &features_obj, &thresholds_obj, &values_obj, &max_depth, &tree_limit, &base_offset_obj, &model_output,
        &X_obj, &X_missing_obj, &y_obj, &out_pred_obj
    )) return NULL;

    /* Interpret the input objects as numpy arrays. */
    PyArrayObject *children_left_array = (PyArrayObject*)PyArray_FROM_OTF(children_left_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_right_array = (PyArrayObject*)PyArray_FROM_OTF(children_right_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *children_default_array = (PyArrayObject*)PyArray_FROM_OTF(children_default_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *features_array = (PyArrayObject*)PyArray_FROM_OTF(features_obj, NPY_INT, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *thresholds_array = (PyArrayObject*)PyArray_FROM_OTF(thresholds_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *values_array = (PyArrayObject*)PyArray_FROM_OTF(values_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *base_offset_array = (PyArrayObject*)PyArray_FROM_OTF(base_offset_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_array = (PyArrayObject*)PyArray_FROM_OTF(X_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *X_missing_array = (PyArrayObject*)PyArray_FROM_OTF(X_missing_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *y_array = NULL;
    if (y_obj != Py_None) y_array = (PyArrayObject*)PyArray_FROM_OTF(y_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *out_pred_array = (PyArrayObject*)PyArray_FROM_OTF(out_pred_obj, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);

    /* If that didn't work, throw an exception. Note that R and y are optional. */
    if (children_left_array == NULL || children_right_array == NULL ||
        children_default_array == NULL || features_array == NULL || thresholds_array == NULL ||
        values_array == NULL || X_array == NULL ||
        X_missing_array == NULL || out_pred_array == NULL) {
        Py_XDECREF(children_left_array);
        Py_XDECREF(children_right_array);
        Py_XDECREF(children_default_array);
        Py_XDECREF(features_array);
        Py_XDECREF(thresholds_array);
        Py_XDECREF(values_array);
        Py_XDECREF(base_offset_array);
        Py_XDECREF(X_array);
        Py_XDECREF(X_missing_array);
        if (y_array != NULL) Py_XDECREF(y_array);
        //PyArray_ResolveWritebackIfCopy(out_pred_array);
        Py_XDECREF(out_pred_array);
        return NULL;
    }

    const unsigned num_X = PyArray_DIM(X_array, 0);
    const unsigned M = PyArray_DIM(X_array, 1);
    const unsigned max_nodes = PyArray_DIM(values_array, 1);
    const unsigned num_outputs = PyArray_DIM(values_array, 2);

    // Get pointers to the data as C-types
    int *children_left = (int*)PyArray_DATA(children_left_array);
    int *children_right = (int*)PyArray_DATA(children_right_array);
    int *children_default = (int*)PyArray_DATA(children_default_array);
    int *features = (int*)PyArray_DATA(features_array);
    tfloat *thresholds = (tfloat*)PyArray_DATA(thresholds_array);
    tfloat *values = (tfloat*)PyArray_DATA(values_array);
    tfloat *base_offset = (tfloat*)PyArray_DATA(base_offset_array);
    tfloat *X = (tfloat*)PyArray_DATA(X_array);
    bool *X_missing = (bool*)PyArray_DATA(X_missing_array);
    tfloat *y = NULL;
    if (y_array != NULL) y = (tfloat*)PyArray_DATA(y_array);
    tfloat *out_pred = (tfloat*)PyArray_DATA(out_pred_array);

    // these are just wrapper objects for all the pointers and numbers associated with
    // the ensemble tree model and the datset we are explaing
    TreeEnsemble trees = TreeEnsemble(
        children_left, children_right, children_default, features, thresholds, values,
        NULL, max_depth, tree_limit, base_offset,
        max_nodes, num_outputs
    );
    ExplanationDataset data = ExplanationDataset(X, X_missing, y, NULL, NULL, num_X, M, 0);

    dense_tree_saabas(out_pred, trees, data);

    // clean up the created python objects 
    Py_XDECREF(children_left_array);
    Py_XDECREF(children_right_array);
    Py_XDECREF(children_default_array);
    Py_XDECREF(features_array);
    Py_XDECREF(thresholds_array);
    Py_XDECREF(values_array);
    Py_XDECREF(base_offset_array);
    Py_XDECREF(X_array);
    Py_XDECREF(X_missing_array);
    if (y_array != NULL) Py_XDECREF(y_array);
    //PyArray_ResolveWritebackIfCopy(out_pred_array);
    Py_XDECREF(out_pred_array);

    /* Build the output tuple */
    PyObject *ret = Py_BuildValue("d", (double)values[0]);
    return ret;
}
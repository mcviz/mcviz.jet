#include <Python.h>

#include <vector>
using std::vector;

#include <iostream>
using std::cout;
using std::endl;

#include "fastjet/Error.hh"
#include "fastjet/PseudoJet.hh"
#include "fastjet/ClusterSequence.hh"

typedef vector<fastjet::PseudoJet> PseudoJetList;

// Global variable, yuck, indeed.
PyObject* PseudoJetType;

PseudoJetList pseudojets_from_pysequence(PyObject* py_input_particle_list)
{
    PseudoJetList pseudojets;
    
    PyObject* iterator = PyObject_GetIter(py_input_particle_list);
    PyObject* particle = NULL;
    while ( (particle = PyIter_Next(iterator)) )
    {
        double px, py, pz, e;
        PyObject* momentum = PyObject_GetAttrString(particle, "p");        
        if (!PyArg_ParseTuple(momentum, "ddd", &px, &py, &pz))
        {
            Py_DECREF(momentum); Py_DECREF(particle);
            PyErr_SetString(PyExc_ValueError, 
                "pseudojets_from_pysequence: Expected momentum variable 'p' to "
                "be three element tuple of floats.");
            return PseudoJetList();
        }
        Py_DECREF(momentum);
        
        PyObject* energy = PyObject_GetAttrString(particle, "e");
        e = PyFloat_AsDouble(energy);
        Py_DECREF(energy);
        
        pseudojets.push_back(fastjet::PseudoJet(px, py, pz, e));
        
        Py_DECREF(particle);
    }
    Py_DECREF(iterator);
    return pseudojets;
}

PyObject *build_jet_objects(PyObject* input_particle_list,  
                            const PseudoJetList& jets, 
                            const vector<int>& particle_jet_indices)
{
    PyObject *result = PyList_New(0);
    
    // A python list per jet, containing the particles which belong to that jet
    vector<PyObject*> jet_particle_pylists;
    
    // Populate the list of jets, their momenta and their energies.
    // A namedtuple per jet is put into the result list.
    for (PseudoJetList::const_iterator i = jets.begin(); 
         i != jets.end(); i++)
    {
        const fastjet::PseudoJet& jet = *i;
        
        PyObject* momentum = Py_BuildValue("(ddd)", jet.px(), jet.py(), jet.pz());
        PyObject* jet_particle_pylist = PyList_New(0);
        
        PyObject* result_jet = PyObject_CallFunction(PseudoJetType, const_cast<char*>("OOd"),
            jet_particle_pylist, momentum, jet.E());
        Py_DECREF(momentum);
           
        // Important: otherwise we get crashes often on ctrl-c.
        if (!result_jet)
            return NULL;
            
        PyList_Append(result, result_jet);
        Py_DECREF(result_jet);
        
        // We decref jet_particle_pylist later
        jet_particle_pylists.push_back(jet_particle_pylist);
    }
    
    // Populate the list of particles that belong to each jet
    //vector<int> particle_jet_indices = clustering.particle_jet_indices(inclusiveJets);
    vector<int>::const_iterator jet_index = particle_jet_indices.begin();
    PyObject* iterator = PyObject_GetIter(input_particle_list);
    PyObject* particle = NULL;
    while ( (particle = PyIter_Next(iterator)) )
    {
        // a jet list index of -1 means no jet.
        int jet_list_index = *(jet_index++);
        if (jet_list_index >= 0)
        {
            PyObject* jet_particle_list = jet_particle_pylists[jet_list_index];
            PyList_Append(jet_particle_list, particle);
        }
        Py_DECREF(particle);
    }
    Py_DECREF(iterator);
    
    // Decref all of the jet particle lists we were holding on to
    for (vector<PyObject*>::iterator i = jet_particle_pylists.begin(); 
         i != jet_particle_pylists.end(); i++)
        Py_DECREF(*i);
        
    return result;
}

static PyObject *cluster_jets(PyObject *self, PyObject *args)
{
    PyObject* input_particle_list = NULL;
    fastjet::JetAlgorithm jet_algorithm = fastjet::kt_algorithm;
    if (!PyArg_ParseTuple(args, "O|i:cluster_jets", 
                          &input_particle_list,
                          &jet_algorithm))
        return NULL;
    
    const PseudoJetList& inputs = pseudojets_from_pysequence(input_particle_list);
    if (PyErr_Occurred())
        return NULL;
    
    try {
        // Algorithm choices
        double Rparam = 0.4;
        fastjet::Strategy               strategy = fastjet::Best;
        fastjet::RecombinationScheme    recombScheme = fastjet::E_scheme;
        fastjet::JetDefinition*         jetDef =
            new fastjet::JetDefinition(jet_algorithm, Rparam,
                                       recombScheme, strategy);
        
        // Run the clustering!
        fastjet::ClusterSequence clustering(inputs, *jetDef);
        
        //cout << "Ran " << jetDef->description() << endl;
        //cout << "Strategy adopted by FastJet was "
        //     << clustering.strategy_string() << endl << endl;

        delete jetDef;
        
        // Get the list of jets.
        const PseudoJetList& jets = clustering.inclusive_jets();
        const vector<int>& particle_jet_indices = clustering.particle_jet_indices(jets);

        PyObject* result = build_jet_objects(input_particle_list, jets, particle_jet_indices);
        return result;
        
    } catch (const fastjet::Error& e)
    {
        PyErr_Format(PyExc_RuntimeError, "fastjet::Error: %s", e.message().c_str());
        return NULL;
    }    
}

static PyMethodDef JetMethods[] = {
    {"cluster_jets", cluster_jets, METH_VARARGS,
     "Run a jet clustering algorithm."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyObject* make_namedtuple(const char* name, const char* fields)
{
    PyObject* collections_module = PyImport_ImportModule("collections");
    PyObject* namedtuple = PyObject_GetAttrString(collections_module, "namedtuple");
    
    PyObject* tupletype = PyObject_CallFunction(
        namedtuple, const_cast<char*>("ss"), name, fields);
        
    Py_DECREF(namedtuple);
    Py_DECREF(collections_module);
    return tupletype;
}

PyMODINIT_FUNC
initfastjet(void)
{
    PyObject *m;

    m = Py_InitModule("fastjet", JetMethods);
    if (m == NULL)
        return;
    
    fastjet::Error::set_print_errors(false);
    
    PseudoJetType = make_namedtuple("PseudoJet", "particles e p");
    if (!PseudoJetType)
        return;
    PyObject_SetAttrString(m, "PseudoJet", PseudoJetType);
    
    PyObject* JetAlgorithmsEnum = make_namedtuple("JetAlgorithmsEnum", "kt_algorithm cambridge_algorithm antikt_algorithm genkt_algorithm cambridge_for_passive_algorithm genkt_for_passive_algorithm ee_kt_algorithm ee_genkt_algorithm");
    if (!JetAlgorithmsEnum)
        return;
        
    PyObject* JetAlgorithms = PyObject_CallFunction(
        JetAlgorithmsEnum, const_cast<char *>("iiiiiiii"),
        fastjet::kt_algorithm,
        fastjet::cambridge_algorithm,
        fastjet::antikt_algorithm,
        fastjet::genkt_algorithm,
        fastjet::cambridge_for_passive_algorithm,
        fastjet::genkt_for_passive_algorithm,
        fastjet::ee_kt_algorithm,
        fastjet::ee_genkt_algorithm);
        
    Py_DECREF(JetAlgorithmsEnum);
    if (!JetAlgorithms)
        return;
        
    PyObject_SetAttrString(m, "JetAlgorithms", JetAlgorithms);
}

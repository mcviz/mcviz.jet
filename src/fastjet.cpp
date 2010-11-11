#include <Python.h>

#include <vector>
using std::vector;

#include <iostream>
using std::cout;
using std::endl;

#include "fastjet/PseudoJet.hh"
#include "fastjet/ClusterSequence.hh"

typedef vector<fastjet::PseudoJet> PseudoJetList;

// Global variable, yuck, indeed.
PyObject* PseudoJetType;

static PyObject *cluster_jets(PyObject *self, PyObject *args)
{
    PyObject* py_input_particle_list = NULL;
    if (!PyArg_ParseTuple(args, "O:cluster_jets", &py_input_particle_list))
        return NULL;
    
    PseudoJetList fjInputs;
    // Make the input list of objects into a 
    PyObject* iterator = PyObject_GetIter(py_input_particle_list);
    PyObject* particle = NULL;
    
    while ( (particle = PyIter_Next(iterator)) )
    {
        double px, py, pz, e;
        PyObject* momentum = PyObject_GetAttrString(particle, "p");        
        if (!PyArg_ParseTuple(momentum, "ddd", &px, &py, &pz))
        {
            Py_DECREF(momentum); Py_DECREF(particle); return NULL;
        }
        Py_DECREF(momentum);
        
        PyObject* energy = PyObject_GetAttrString(particle, "e");
        e = PyFloat_AsDouble(energy);
        Py_DECREF(energy);
        
        fjInputs.push_back(fastjet::PseudoJet(px, py, pz, e));
        
        Py_DECREF(particle);
    }
    Py_DECREF(iterator);
    
    // Algorithm choices
    double Rparam = 0.4;
    fastjet::Strategy               strategy = fastjet::Best;
    fastjet::RecombinationScheme    recombScheme = fastjet::E_scheme;
    fastjet::JetDefinition         *jetDef =
        new fastjet::JetDefinition(fastjet::kt_algorithm, Rparam,
                                   recombScheme, strategy);
    
    // Run the clustering!
    fastjet::ClusterSequence clustering(fjInputs, *jetDef);
    
    //cout << "Ran " << jetDef->description() << endl;
    //cout << "Strategy adopted by FastJet was "
    //     << clustering.strategy_string() << endl << endl;

    delete jetDef;

    // Get the list of jets.
    PseudoJetList inclusiveJets = clustering.inclusive_jets();

    // Python list to store the result
    PyObject *result = PyList_New(0);
    
    // A python list per jet, containing the particles which belong to that jet
    vector<PyObject*> jet_particle_pylists;
    
    // Populate the list of jets, their momenta and their energies.
    // A namedtuple per jet is put into the result list.
    for (PseudoJetList::iterator i = inclusiveJets.begin(); 
         i != inclusiveJets.end(); i++)
    {
        fastjet::PseudoJet& jet = *i;
        
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
    vector<int> particle_jet_indices = clustering.particle_jet_indices(inclusiveJets);
    vector<int>::iterator jet_index = particle_jet_indices.begin();
    iterator = PyObject_GetIter(py_input_particle_list);
    while ( (particle = PyIter_Next(iterator)) )
    {
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

static PyMethodDef JetMethods[] = {
    {"cluster_jets", cluster_jets, METH_VARARGS,
     "Run a jet clustering algorithm."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


void make_pseudojet_namedtuple()
{
    PyObject* collections_module = PyImport_ImportModule("collections");
    PyObject* namedtuple = PyObject_GetAttrString(collections_module, "namedtuple");
    
    PseudoJetType = PyObject_CallFunction(namedtuple, const_cast<char*>("ss"), "PseudoJet", "particles p e");
    
    Py_DECREF(namedtuple);
    Py_DECREF(collections_module);
    // Hold a reference to PseudoJetType always.
}

PyMODINIT_FUNC
initfastjet(void)
{
    PyObject *m;

    m = Py_InitModule("fastjet", JetMethods);
    if (m == NULL)
        return;
        
    make_pseudojet_namedtuple();
    PyObject_SetAttrString(m, "PseudoJet", PseudoJetType);
    
}

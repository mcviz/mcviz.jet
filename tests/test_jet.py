from mcviz.loaders import hepmc 
from mcviz.jet import cluster_jets

from sys import getrefcount

# Note that these tests expect
#r /usr/bin/nosetests -sv mcviz.jet

def get_final_state_particles():
    
    vertices, particles = hepmc.load_first_event("pythia01.hepmc")    
    final_particles = [p for p in particles.values() if p.final_state]
    return final_particles

def test_jet_algorithm():
    """
    Performing a basic test of cluster_jets
    """
    final_particles = get_final_state_particles()
    
    jets = cluster_jets(final_particles)
    print len(final_particles)
    print len(jets)
    
def test_for_leaks():
    """
    Leak testing cluster_jets
    """
    final_particles = get_final_state_particles()
    
    def get_refstate():
        return getrefcount(final_particles), map(getrefcount, final_particles)
    
    first_refstate = get_refstate()
    
    for i in xrange(100):
        jets = cluster_jets(final_particles)
        # Two because getrefcount() has a reference.
        assert getrefcount(jets) == 2
        # If this list goes away the refcounts should be back to 
        # where they started.
        del jets
        # Check that we're not obviously leaking.
        assert get_refstate() == first_refstate, "Leaking objects!"

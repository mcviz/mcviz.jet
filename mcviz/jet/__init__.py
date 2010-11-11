from fastjet import cluster_jets, PseudoJet

def PseudoJet__repr__(self):
    args = self.p + (self.e, len(self.particles))
    return "<PseudoJet p=(%.2f, %.2f, %.2f) E=%.2f nparticles=%i>" % args
    


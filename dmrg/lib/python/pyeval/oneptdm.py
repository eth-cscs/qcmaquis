#!/usr/bin/env python
# -*- coding: utf-8 -*-
#*****************************************************************************
#*
#* ALPS MPS DMRG Project
#*
#* Copyright (C) 2013 Laboratory for Physical Chemistry, ETH Zurich
#*               2012-2014 by Sebastian Keller <sebkelle@phys.ethz.ch>
#*
#* 
#* This software is part of the ALPS Applications, published under the ALPS
#* Application License; you can use, redistribute it and/or modify it under
#* the terms of the license, either version 1 or (at your option) any later
#* version.
#* 
#* You should have received a copy of the ALPS Application License along with
#* the ALPS Applications; see the file LICENSE.txt. If not, the license is also
#* available from http://alps.comp-phys.org/.
#*
#* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
#* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
#* FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
#* SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
#* FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
#* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
#* DEALINGS IN THE SOFTWARE.
#*
#*****************************************************************************


import sys
import numpy as np
import scipy.linalg as sl
from copy import deepcopy

import maquisFile

from corrutils import assemble_halfcorr as assy_hc
from corrutils import assemble_vector as assy_vec
from corrutils import pretty_print

def read_irreps(orbfile):
    of = open(orbfile, 'r')
    orbstring = of.readlines(1000)[1]

    substring = orbstring.split('=')[1]
    ret = map(int, [ s for s in substring.split(',') if s.strip()])
    return ret

def diag_dm(matrix):

    evals = sl.eigh(matrix)[0][::-1]
    for e in evals:
            print("{0: .5f}".format(e))

class oneptdm:
    def __init__(self, inputfile):
        # load data from the HDF5 result file
        self.nup    = assy_vec(maquisFile.loadEigenstateMeasurements([inputfile], what='Nup')[0][0])
        self.ndown  = assy_vec(maquisFile.loadEigenstateMeasurements([inputfile], what='Ndown')[0][0])
        self.dmup   = assy_hc(self.nup, maquisFile.loadEigenstateMeasurements([inputfile], what='dm_up')[0][0])
        self.dmdown = assy_hc(self.ndown, maquisFile.loadEigenstateMeasurements([inputfile], what='dm_down')[0][0])

    def rdm(self):
        return self.rdm_a() + self.rdm_b()

    def rdm_a(self):
        return deepcopy(self.dmup)

    def rdm_b(self):
        return deepcopy(self.dmdown)

if __name__ == '__main__':
    inputfile = sys.argv[1]

    dm_ = oneptdm(inputfile)
    dm = dm_.rdm()
    pretty_print(dm)

    blocks = [len(dm)]
    if len(sys.argv) == 3:
        orbfile = sys.argv[2]
        irrlist = read_irreps(orbfile)
        blocks = np.bincount(irrlist)[1:]

    print("Point group blocks", blocks)

    bstart = 0
    for l in blocks:
        subblock = dm[bstart:bstart+l, bstart:bstart+l]
        diag_dm(subblock)
        bstart += l

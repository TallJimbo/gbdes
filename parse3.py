#!/sw/bin/python2.7

import pyfits
import glob
import re
import os
import sys
from types import *
from collections import defaultdict
from pyfits import Column
import numpy as np
from copy import deepcopy
class Enum(tuple): 
    __getattr__ = tuple.index

        

#   These specify allowable catalog types

# These are reserved names in the config files
Assigned =['Params','AddFiles','Outfile']

# These are a list properties every extension has

Internal =['FILENAME','EXTENSION','INSTRUMENT','DEVICE',
           'FIELD','EXPOSURE','RA','DEC','WCSFILE','XKEY',
           'YKEY','IDKEY']
IDefault ={'FILENAME':'','EXTENSION':-1,'INSTRUMENT':'','DEVICE':'',
           'FIELD':'@_NEAREST','EXPOSURE':'','RA':'','DEC':'',
           'WCSFILE':'','XKEY':'',
           'YKEY':'','IDKEY':'@_ROW'}


def py_to_fits(val):
    """
    Return a fits type to be used in file creation based
    on a python type.  Use the type of the first entry
    in the list
    """

    vtype=type(val[0])
    
    if(vtype is IntType):
        return 'J'
    elif(vtype is FloatType):
        return 'D'
    elif(vtype is StringType):
        size=len(max(val,key=len))
        return 'A'+str(size)


class Param:
    """
    Simple class to hold parameter values
    """
    def __init__(self,name='',value=''):
        self.name=name
        self.value=value


class ExposureExt:
    """ 
    
    Class for each extenstion of each exposure
    It contains a set of parameters that every exposure/ext must have
    and another list of Params which can can vary
    
    """
    def __init__(self,param_list=[]):
        self.param=[]
        for p in Internal:
            self.param.append(Param(p,IDefault[p]))
        

        for param in param_list:
            self.param.append(Param(param))

    def setVal(self,key,val):
        for item in self.param:
            if key == item.name:
                if key == 'EXTENSION':
                    item.value=int(val)
                    return
                item.value=val
                return
        raise ValueError('No key: '+key)
        

    def getVal(self,key):

        for item in self.param:
            if key == item.name:
                return item.value
        raise ValueError('No key: '+key)



def getConfig(filename):
    """
    Read the configuration file and return the parameters
    and both global and local value properties
    """

    fp = open(filename)
    lines = fp.readlines()
    fp.close()
    filelist=''
    map = {}
    global_p = {}
    local_p=defaultdict(dict)

    comment=re.compile(r'^\s*#.*')
    blank=re.compile(r'^\s*$')
    equal=re.compile(r'^\s*(\S*)\s*=\s*(\S.*)\s*')
    assign=re.compile(r'^\s*(\S*)\s+(\S.*)\s*')

    for line in lines:

        m=blank.match(line)
        if m: continue 

        m=comment.match(line)
        if m: continue 
        
        m=equal.match(line)
        m2=assign.match(line)
        if m or m2:
            if m: 
                key=m.group(1)
                val=m.group(2)
            else:
                key=m2.group(1)
                val=m2.group(2)


            if key=='AddFiles':
                if filelist == '': filelist=val
                else : filelist=filelist+':'+val
                continue
            
            if val=="''" or val=="\"\"": val=''
            if key in Assigned:
                map[key]=val
                continue

            # is it a global
            sub=re.compile(r'(.*)\[(.*)\]')
            msub=sub.match(key)
            if not msub:
                global_p[key]=val
                continue
            elif msub:
                regex=msub.group(1)
                k=msub.group(2)
                local_p[regex][k]=val
                continue
            
            
        raise IOError('Cannot read: '+line)

    return map,filelist,global_p,local_p



def setInitExp(exps,regex,alist,param):
    """
    Given a list of extensions and a regular expresion (regex)
    assign all files with matching filename the parameters from param
    """
    # make sure all params are in param list
    #refmatch=re.compile(r'\${(.*)}')
    refmatch='\${(\w*)}'
    for key,val in param.items():
        match_list=re.findall(refmatch,val)
        for m in match_list: 
            if m not in alist and m not in Internal :
                raise ValueError('Cannot set '+
                                 m+' not in Params list')
            continue
        if key not in alist and key not in Internal :
            raise ValueError('Cannot set '+key+' not in Params list')

    #head=re.compile(r'Header')
    filematch=re.compile(regex)


    for exp in exps:
        if not filematch.search(exp.getVal('FILENAME')): continue
        
        # list for parameters that reference other parameters
        # the first entry in map will be the reference parameter
        # and the string with 
        wait={}#defaultdict(list)
        for key,val in param.items():
            match_list=re.findall(refmatch,val)
            if len(match_list)>0:
                wait[key]=val
            #    sub_string=val
            #    for i,m in enumerate(match_list):
            #        sub_string=re.sub(m,'XXXX%d'%i,sub_string)
            #        wait[key][.append(m)
            #    wait[key].append(sub_string)
            #    continue
            exp.setVal(key,val)

        for key,val in wait.items():
            match_list=re.findall(refmatch,val)
            for m in match_list:
                par_val=str(exp.getVal(m))
                val=re.sub('\${%s}'%m,par_val,val)
            exp.setVal(key,val)

def buildExpList(flist):
    """
    Build the exposure list from an initial list of files only
    This will assign the correct extension and expand to one
    entry per extension if file is an MEF
    """
    exps=[]

    for exp in flist:
        basefile=os.path.basename(exp.getVal('FILENAME'))
        m=re.search(r'(.*)\.\S*\b',basefile)

        if not m:
            raise ValueError('Cannot match name from file: '+file)
        name=m.group(1)
        # remove _cat if it is there
        name=re.sub('_cat','',name)

        # remove trailing ccd number
        name=re.sub('_\d\d$','',name)
        
        exp.setVal('EXPOSURE',name)
        exps.append(exp)
        
    return exps


if len(sys.argv)==1:
    print 'Must specify input file'
    exit(1)
map,filelist,global_p,local_p=getConfig(sys.argv[1])


if 'Params' in map:
    eparams=re.split(',',map['Params'])

# get file list that match
files=[]
filematch=re.split(':',filelist)
for entry in filematch:
    potential=glob.glob(entry)
    if len(potential) == 0:
        print 'Warning: no files found match '+entry
    else:
        files.extend(glob.glob(entry))


# set non header properties
flist=[]
for file in files:
    ext=ExposureExt(param_list=eparams)
    ext.setVal('FILENAME',file)
    flist.append(ext)

exps=buildExpList(flist)

setInitExp(flist,'.*',eparams,global_p)
for regex,p in local_p.items():
    setInitExp(flist,regex,eparams,p)





vars=deepcopy(Internal)
# in case we want to remove any internal parameters we can do
# it here
vars.extend(eparams)

vals=defaultdict(list)
for exp in exps:
    for i,v in enumerate(vars):
        val=exp.getVal(v)
        vals[v].append(val)



cols=[]
for k, v in vals.items():
    vtype=py_to_fits(v)

    # convert all entries to desired type
    if vtype == 'A0': vtype='A1'
    if vtype=='D':
        tv=[float(s) for s in v]
    elif vtype=='J':
        tv=[int(s) for s in v]
    else:
        tv=[str(s) for s in v]
    varray=np.array(tv)

    col=Column(name=k, format=vtype, array=varray)
    
    cols.append(col)

outname='test.fits'
if 'Outfile' in map:
    outname=map['Outfile']

pycols = pyfits.ColDefs(cols)
hdu = pyfits.new_table(pycols)
hdu.writeto(outname, clobber=True)


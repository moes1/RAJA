#!/usr/bin/env python
#
# Copyright (c) 2016, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory.
#
# All rights reserved.
#
# This source code cannot be distributed without permission and
# further review from Lawrence Livermore National Laboratory.
#


import sys
from itertools import permutations
from lperm import *

def writeEnumDecl(ndims_list):
  for ndims in ndims_list:
    # Get names of each permutation    
    enum_names = getEnumNames(ndims)
    
    # Write an enum for each permutation
    for enum in enum_names:
      print "struct %s {};" % enum          
    continue
    
  print ""

def writeLayoutDecl(ndims_list):

  for ndims in ndims_list:
    dim_names = getDimNames(ndims)
  
    args = map(lambda a: "typename Idx%s=Index_type"%a.upper(), dim_names)
    argstr = ", ".join(args)
    print "template<typename Perm, %s, typename IdxLin=Index_type>" % argstr
    print "struct Layout%dd {};" % ndims
    print 
       
 
  
def writeLayoutImpl(ndims_list):

  for ndims in ndims_list:
    dim_names = getDimNames(ndims)
    
    print ""
    print "/******************************************************************"
    print " *  Implementation for Layout%dD" % ndims
    print " ******************************************************************/"
    print ""
                
    # Loop over each permutation specialization
    perms = getDimPerms(dim_names)
    for perm in perms:
      # get enumeration name
      enum = getEnumName(perm)
      
      # Start the partial specialization
      args = map(lambda a: "typename Idx%s"%a.upper(), dim_names)
      argstr = ", ".join(args)
      print "template<%s, typename IdxLin>" % argstr
      
      args = map(lambda a: "Idx%s"%a.upper(), dim_names)
      argstr = ", ".join(args)
      print "struct Layout%dd<%s, %s, IdxLin> {" % (ndims, enum, argstr)
    
      # Create typedefs to capture the template parameters
      print "  typedef %s Permutation;" % enum
      print "  typedef IdxLin IndexLinear;"
      for a in dim_names:
        print "  typedef Idx%s Index%s;" % (a.upper(), a.upper())
      print ""


      # Add local variables
      args = map(lambda a: "Index_type const size_"+a, dim_names)
      for arg in args:
        print "  %s;" % arg
        
      # Add stride variables
      print ""
      args = map(lambda a: "Index_type const stride_"+a, dim_names)
      for arg in args:
        print "  %s;" % arg
      print ""
    
      # Define constructor
      args = map(lambda a: "Index_type n"+a, dim_names)
      argstr = ", ".join(args)    
      print "  inline Layout%dd(%s):" % (ndims, argstr)    
      
      # initialize size of each dim
      args = map(lambda a: "size_%s(n%s)"%(a,a), dim_names)
      
      # initialize stride of each dim      
      for i in range(0,ndims):
        remain = perm[i+1:]
        if len(remain) > 0:
          remain = map(lambda a: "n"+a, remain)
          stride = "stride_%s(%s)" % ( perm[i],  "*".join(remain) )          
        else:
          stride = "stride_%s(1)" % ( perm[i] )
        args.append(stride)
      args.sort()
          
      # output all initializers
      argstr = ", ".join(args)
      print "    %s" % argstr                    
      print "  {}"
      print ""
      
      

      # Define () Operator, the indices -> linear function
      args = map(lambda a: "Idx%s %s"%(a.upper(), a) , dim_names)
      argstr = ", ".join(args)   
      idxparts = []
      for i in range(0,ndims):
        remain = perm[i+1:]        
        if len(remain) > 0:
          idxparts.append("convertIndex<Index_type>(%s)*stride_%s" % (perm[i], perm[i]))
        else:
          idxparts.append("convertIndex<Index_type>(%s)" % perm[i])
      idx = " + ".join(idxparts)  

      print "  inline IdxLin operator()(%s) const {" % (argstr)
      print "    return convertIndex<IdxLin>(" + idx + ");"
      print "  }"
      print ""
               
               
                 
      # Define the linear->indices functions      
      args = map(lambda a: "Idx%s &%s"%(a.upper(), a), dim_names)
      argstr = ", ".join(args)
      print "  inline void toIndices(IdxLin lin, %s) const {" % (argstr)
      print "    Index_type linear = convertIndex<Index_type>(lin);"
      for i in range(0, ndims):
        idx = perm[i]
        prod = "*".join(map(lambda a: "size_%s"%a, perm[i+1 : ndims]))
        if prod != '':
          print "    Index_type _%s = linear / (%s);" % (idx, prod)
          print "    %s = Idx%s(_%s);" % (idx, idx.upper(), idx)
          print "    linear -= _%s*(%s);" % (idx, prod)
      print "    %s = Idx%s(linear);" % (perm[ndims-1], perm[ndims-1].upper()) 
      print "  }"
      
      # Close out class
      print "};"

      print ""    
      print ""          



def main(ndims):
  print """//AUTOGENERATED BY genLayout.py
  /*
   * Copyright (c) 2016, Lawrence Livermore National Security, LLC.
   * Produced at the Lawrence Livermore National Laboratory.
   *
   * All rights reserved.
   *
   * This source code cannot be distributed without permission and
   * further review from Lawrence Livermore National Laboratory.
   */
    
  #ifndef RAJA_LAYOUT_HXX__
  #define RAJA_LAYOUT_HXX__

  #include <RAJA/IndexValue.hxx>

  namespace RAJA {

  """

  ndims_list = range(1,ndims+1)

  # Dump all declarations (with documentation, etc)
  writeEnumDecl(ndims_list)
  writeLayoutDecl(ndims_list)

  # Dump all implementations and specializations
  writeLayoutImpl(ndims_list)

  print """

  } // namespace RAJA

  #endif
  """

if __name__ == '__main__':
  main(int(sys.argv[1]))
  

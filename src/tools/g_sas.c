/*
 *       $Id$
 *
 *       This source code is part of
 *
 *        G   R   O   M   A   C   S
 *
 * GROningen MAchine for Chemical Simulations
 *
 *            VERSION 2.0
 * 
 * Copyright (c) 1991-1997
 * BIOSON Research Institute, Dept. of Biophysical Chemistry
 * University of Groningen, The Netherlands
 * 
 * Please refer to:
 * GROMACS: A message-passing parallel molecular dynamics implementation
 * H.J.C. Berendsen, D. van der Spoel and R. van Drunen
 * Comp. Phys. Comm. 91, 43-56 (1995)
 *
 * Also check out our WWW page:
 * http://rugmd0.chem.rug.nl/~gmx
 * or e-mail to:
 * gromacs@chem.rug.nl
 *
 * And Hey:
 * Green Red Orange Magenta Azure Cyan Skyblue
 */
static char *SRCID_g_sas_c = "$Id$";

#include <math.h>
#include <stdlib.h>
#include "sysstuff.h"
#include "string.h"
#include "typedefs.h"
#include "smalloc.h"
#include "macros.h"
#include "vec.h"
#include "xvgr.h"
#include "pbc.h"
#include "copyrite.h"
#include "futil.h"
#include "statutil.h"
#include "rdgroup.h"
#include "nsc.h"
#include "vdw.h"
#include "pdbio.h"
#include "confio.h"
#include "rmpbc.h"

void connelly_plot(char *fn,int ndots,real dots[],rvec x[],t_atoms *atoms,
		   t_symtab *symtab,matrix box)
{
  static char *atomnm="DOT";
  static char *resnm ="DOT";
  static char *title ="Connely Dot Surface Generated by g_sas";

  int  i,i0,ii0,k;
  rvec *xnew;
  
  i0 = atoms->nr;
  srenew(atoms->atom,atoms->nr+ndots);
  srenew(atoms->atomname,atoms->nr+ndots);
  srenew(atoms->resname,atoms->nr+ndots);
  srenew(atoms->pdbinfo,atoms->nr+ndots);
  snew(xnew,atoms->nr+ndots);
  for(i=0; (i<atoms->nr); i++)
    copy_rvec(x[i],xnew[i]);
  for(i=k=0; (i<ndots); i++) {
    ii0 = i0+i;
    atoms->resname[ii0]  = put_symtab(symtab,resnm);
    atoms->atomname[ii0] = put_symtab(symtab,atomnm);
    strcpy(atoms->pdbinfo[ii0].pdbresnr,"1");
    atoms->pdbinfo[ii0].type = epdbATOM;
    atoms->atom[ii0].chain = ' ';
    atoms->pdbinfo[ii0].atomnr= ii0;
    atoms->atom[ii0].resnr = 1;
    xnew[ii0][XX] = dots[k++];
    xnew[ii0][YY] = dots[k++];
    xnew[ii0][ZZ] = dots[k++];
    atoms->pdbinfo[ii0].bfac  = 0.0;
    atoms->pdbinfo[ii0].occup = 0.0;
  }
  atoms->nr = i0+ndots;
  write_sto_conf(fn,title,atoms,xnew,NULL,box);
  atoms->nr = i0;
  
  sfree(xnew);
}

real calc_radius(char *atom)
{
  real r;
  
  switch (atom[0]) {
  case 'C':
    r = 0.16;
    break;
  case 'O':
    r = 0.13;
    break;
  case 'N':
    r = 0.14;
    break;
  case 'S':
    r = 0.2;
    break;
  case 'H':
    r = 0.1;
    break;
  default:
    r = 1e-3;
  }
  return r;
}

void sas_plot(int nfile,t_filenm fnm[],real solsize,real defvdw,int ndots,
	      real qcut)
{
  FILE         *fp;
  real         t;
  int          nvdw,status;
  int          i,j,natoms,flag,nsurfacedots;
  rvec         *x;
  matrix       box;
  /*  t_vdw        *vdw; */
  t_topology   *top;
  bool         *bPhobic;
  bool         bConnelly;
  real         *radius,*area,*surfacedots;
  real         totarea,totvolume,harea;
    
  if ((natoms=read_first_x(&status,ftp2fn(efTRX,nfile,fnm),
			   &t,&x,box))==0)
    fatal_error(0,"Could not read coordinates from statusfile\n");
  top=read_top(ftp2fn(efTPX,nfile,fnm));

  /*
  nvdw = read_vdw(ftp2fn(efVDW,nfile,fnm),&vdw);
  fprintf(stderr,"There are %d VanderWaals radii\n",nvdw);
  */

  /* Now comput atomic readii including solvent probe size */
  snew(radius,natoms);
  snew(bPhobic,natoms);
  for(i=0; (i<natoms); i++) {
    radius[i]  = calc_radius(*(top->atoms.atomname[i])) + solsize;
    /*get_vdw(nvdw,vdw,*(top->atoms.atomname[i])) + solsize;*/
    bPhobic[i] = fabs(top->atoms.atom[i].q) <= qcut;
    /*(*(top->atoms.atomname[i]))[0] == 'C';*/
  }
  fp=xvgropen(ftp2fn(efXVG,nfile,fnm),"Solvent Accessible Surface","Time (ps)",
	      "Area (nm\\S2\\N)");
  
  j=0;
  do {
    if ((j++ % 10) == 0)
      fprintf(stderr,"\rframe: %5d",j-1);
      
    rm_pbc(&top->idef,natoms,box,x,x);

    bConnelly = ((j == 1) && (opt2bSet("-q",nfile,fnm)));
    if (bConnelly)
      flag = FLAG_ATOM_AREA | FLAG_DOTS;
    else
      flag = FLAG_ATOM_AREA;
    
    if (NSC(x[0],radius,natoms,ndots,flag,&totarea,
	    &area,&totvolume,&surfacedots,&nsurfacedots))
      fatal_error(0,"Something wrong in NSC");
      
    if (bConnelly)
      connelly_plot(ftp2fn(efPDB,nfile,fnm),
		    nsurfacedots,surfacedots,x,&(top->atoms),
		    &(top->symtab),box);
      
    harea = 0;
    for(i=0; (i<natoms); i++) {
      if (bPhobic[i])
	harea += area[i];
    }
    fprintf(fp,"%10g  %10g  %10g\n",t,harea,totarea);
    
    sfree(area);
    sfree(surfacedots);
      
  } while (read_next_x(status,&t,natoms,x,box));
  
  fprintf(stderr,"\n");
  close_trj(status);

  sfree(x);
}

int main(int argc,char *argv[])
{
  static char *desc[] = {
    "g_sas computes hydrophobic and total solvent accessible surface area."
  };

  static real solsize = 0.14;
  static real defvdw  = 0.15;
  static int  ndots   = 24;
  static real qcut    = 0.2;
  t_pargs pa[] = {
    { "-solsize", FALSE, etREAL, &solsize,
	"Radius of the solvent probe (nm)" },
    { "-defvdw",  FALSE, etREAL, &defvdw,
	"Default Van der Waals radius for unknown atoms" },
    { "-ndots",   FALSE, etINT,  &ndots,
	"Number of dots per sphere, more dots means more accuracy" },
    { "-qmax",    FALSE, etREAL, &qcut,
	"The maximum charge (e, absolute value) of a hydrophobic atom" }
  };
  t_filenm  fnm[] = {
    { efTRX, "-f",   NULL,       ffREAD },
    { efTPX, "-s",   NULL,       ffREAD },
    /*    { efVDW, "-vdw", NULL,       ffREAD }, */
    { efXVG, "-o",   "area",     ffWRITE },
    { efPDB, "-q",   "connelly", ffOPTWR }
  };
#define NFILE asize(fnm)

  CopyRight(stderr,argv[0]);
  parse_common_args(&argc,argv,PCA_CAN_VIEW | PCA_CAN_TIME,TRUE,
		    NFILE,fnm,asize(pa),pa,asize(desc),desc,0,NULL);
  if (solsize <= 0) {
    solsize=1e-3;
    fprintf(stderr,"Solsize too small, setting it to %g\n",solsize);
  }
  if (ndots < 20) {
    ndots = 20;
    fprintf(stderr,"Ndots too small, setting it to %d\n",ndots);
  }
  
  sas_plot(NFILE,fnm,solsize,defvdw,ndots,qcut);
  
  xvgr_file(opt2fn("-o",NFILE,fnm),"-nxy");
  
  thanx(stdout);
  
  return 0;
}


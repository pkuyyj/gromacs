/*
 * $Id$
 * 
 *       This source code is part of
 * 
 *        G   R   O   M   A   C   S
 * 
 * GROningen MAchine for Chemical Simulations
 * 
 *               VERSION 2.0
 * 
 * Copyright (c) 1991-1999
 * BIOSON Research Institute, Dept. of Biophysical Chemistry
 * University of Groningen, The Netherlands
 * 
 * Please refer to:
 * GROMACS: A message-passing parallel molecular dynamics implementation
 * H.J.C. Berendsen, D. van der Spoel and R. van Drunen
 * Comp. Phys. Comm. 91, 43-56 (1995)
 * 
 * Also check out our WWW page:
 * http://md.chem.rug.nl/~gmx
 * or e-mail to:
 * gromacs@chem.rug.nl
 * 
 * And Hey:
 * Great Red Oystrich Makes All Chemists Sane
 */
static char *SRCID_g_sgangle_c = "$Id$";

#include <math.h>
#include "sysstuff.h"
#include "string.h"
#include "typedefs.h"
#include "smalloc.h"
#include "macros.h"
#include "lutab.h"
#include "rmpbc.h"
#include "vec.h"
#include "xvgr.h"
#include "pbc.h"
#include "copyrite.h"
#include "futil.h"
#include "statutil.h"
#include "rdgroup.h"

/* this version only works correctly if one of the entries in the index file
   is a plane (three atoms specified) and the other a vector. Distance
   is calculated from the center of the plane to both atoms of the vector */

static void print_types(atom_id index1[], int gnx1, char *group1,
		       atom_id index2[], int gnx2, char *group2,
		       t_topology *top)
{
  int i,j;

  fprintf(stderr,"\n");
  fprintf(stderr,"Group %s contains the following atoms: \n",group1);
  for(i=0;i<gnx1;i++)
    fprintf(stderr,"Atomname %d: %s\n",i,*(top->atoms.atomname[index1[i]]));
  fprintf(stderr,"\n");
  
  fprintf(stderr,"Group %s contains the following atoms: \n",group2);
  for(j=0;j<gnx2;j++)    
    fprintf(stderr,"Atomname %d: %s\n",j,*(top->atoms.atomname[index2[j]]));
  fprintf(stderr,"\n");

  fprintf(stderr,"Careful: distance only makes sense in some situations.\n\n");
}

static void calculate_normal(atom_id index[],rvec x[],rvec result,rvec center)
{
  rvec c1,c2;
  int i;
  
  /* calculate centroid of triangle spanned by the three points */
  for(i=0;i<3;i++)
    center[i] = (x[index[0]][i] + x[index[1]][i] + x[index[2]][i])/3;
  
  /* use P1P2 x P1P3 to calculate normal, given three points P1-P3 */
  rvec_sub(x[index[1]],x[index[0]],c1);    /* find two vectors */
  rvec_sub(x[index[2]],x[index[0]],c2);
  
  oprod(c1,c2,result);                    /* take crossproduct between these */
}

/* calculate the angle and distance between the two groups */
static void calc_angle(matrix box,rvec x[], atom_id index1[], 
		       atom_id index2[], int gnx1, int gnx2,
		       real *angle,      real *distance, 
		       real *distance1,  real *distance2)

/* distance is distance between centers, distance 1 between center of plane
   and atom one of vector, distance 2 same for atom two
*/

{
  rvec 
    normal1,normal2,  	/* normals on planes of interest */
    center1,center2,  	/* center of triangle of points given to define plane,*/
                      	/* or center of vector if a vector is given */
    h1,h2,h3,h4,h5;  	/* temp. vectors */
  
  switch(gnx1)
    {
    case 3:           /* group 1 defines plane */
      calculate_normal(index1,x,normal1,center1);
      break;
    case 2:           /* group 1 defines vector */
      rvec_sub(x[index1[0]],x[index1[1]],normal1);
      rvec_add(x[index1[0]],x[index1[1]],h1);
      svmul(0.5,h1,center1);  /* center is geometric mean */
      break;
    default:          /* group 1 does none of the above */
      fatal_error(0,"Something wrong with contents of index file.\n");
    }

  switch(gnx2)
    {
    case 3:          /* group 2 defines plane */
      calculate_normal(index2,x,normal2,center2);
      break;
    case 2:          /* group 2 defines vector */
      rvec_sub(x[index2[0]],x[index2[1]],normal2);
      rvec_add(x[index2[0]],x[index2[1]],h2);
      svmul(0.5,h2,center2);  /* center is geometric mean */
      break;
    default:         /* group 2 does none of the above */
      fatal_error(0,"Something wrong with contents of index file.\n");
    }
  
  *angle = cos_angle(normal1,normal2);

  rvec_sub(center1,center2,h3); 
  *distance = norm(h3);

  if (gnx1 == 3 && gnx2 == 2) {
    rvec_sub(center1,x[index2[0]],h4);
    rvec_sub(center1,x[index2[1]],h5);
    *distance1 = norm(h4);
    *distance2 = norm(h5);
  }
  else if (gnx1 == 2 && gnx2 ==3) {
    rvec_sub(center1,x[index1[0]],h4);
    rvec_sub(center1,x[index1[1]],h5);
    *distance1 = norm(h4);
    *distance2 = norm(h5);
  }
  else {
    *distance1 = 0; *distance2 = 0;
  } 

}

void sgangle_plot(char *fn,char *afile,char *bfile, 
		  char *cfile, char *dfile,
		  atom_id index1[], int gnx1, char *grpn1,
		  atom_id index2[], int gnx2, char *grpn2,
		  t_topology *top)
{
  FILE         
    *sg_angle,           /* xvgr file with angles */
    *sg_distance,        /* xvgr file with distances */
    *sg_distance1,       /* xvgr file with distance between plane and atom */
    *sg_distance2;       /* xvgr file with distance between plane and atom2 */
  real         
    t,                   /* time */
    angle,               /* cosine of angle between two groups */
    distance,            /* distance between two groups. */
    distance1,           /* distance between plane and one of two atoms */
    distance2;           /* same for second of two atoms */
  int        status,natoms,teller=0;
  rvec       *x0;   /* coordinates, and coordinates corrected for pb */
  matrix     box;        
  char       buf[256];   /* for xvgr title */

  if ((natoms = read_first_x(&status,fn,&t,&x0,box)) == 0)
    fatal_error(0,"Could not read coordinates from statusfile\n");

  sprintf(buf,"Angle between %s and %s",grpn1,grpn2);
  sg_angle = xvgropen(afile,buf,"Time (ps)","Cos(angle) ");

  sprintf(buf,"Distance between %s and %s",grpn1,grpn2);
  sg_distance = xvgropen(bfile,buf,"Time (ps)","Distance (nm)");

  sprintf(buf,"Distance between plane and first atom of vector");
  sg_distance1 = xvgropen(cfile,buf,"Time (ps)","Distance (nm)");

  sprintf(buf,"Distance between plane and second atom of vector");
  sg_distance2 = xvgropen(dfile,buf,"Time (ps","Distance (nm)");

  do 
    {
      teller++;

      rm_pbc(&(top->idef),top->atoms.nr,box,x0,x0);
      
      calc_angle(box,x0,index1,index2,gnx1,gnx2,&angle,
		 &distance,&distance1,&distance2);
      
      fprintf(sg_angle,"%12g  %12g  %12g\n",t,angle,acos(angle)*180.0/M_PI);
      fprintf(sg_distance,"%12g  %12g\n",t,distance);
      fprintf(sg_distance1,"%12g  %12g\n",t,distance1);
      fprintf(sg_distance2,"%12g  %12g\n",t,distance1);

    } while (read_next_x(status,&t,natoms,x0,box));
  
  fprintf(stderr,"\n");
  close_trj(status);
  fclose(sg_angle);
  fclose(sg_distance);
  fclose(sg_distance1);
  fclose(sg_distance2);

  sfree(x0);
}

int main(int argc,char *argv[])
{
  static char *desc[] = {
    "Compute the angle and distance between two groups. ",
    "The groups are defined by a number of atoms given in an index file and",
    "may be two or three atoms in size.",
    "The angles calculated depend on the order in which the atoms are ",
    "given. Giving for instance 5 6 will rotate the vector 5-6 with ",
    "180 degrees compared to giving 6 5. [PAR]If three atoms are given, ",
    "the normal on the plane spanned by those three atoms will be",
    "calculated, using the formula  P1P2 x P1P3.",
    "The cos of the angle is calculated, using the inproduct of the two",
    "normalized vectors.[PAR]",
    "Here is what some of the file options do:[BR]",
    "-oa: Angle between the two groups specified in the index file. If a group contains three atoms the normal to the plane defined by those three atoms will be used. If a group contains two atoms, the vector defined by those two atoms will be used.[BR]",
    "-od: Distance between two groups. Distance is taken from the center of one group to the center of the other group.[BR]",
    "-od1: If one plane and one vector is given, the distances for each of the atoms from the center of the plane is given seperately.[BR]",
    "-od2: For two planes this option has no meaning."
  };

  char      *grpname[2];          		/* name of the two groups */
  int       gnx[2];               		/* size of the two groups */
  t_topology *top;                		/* topology 		*/ 
  atom_id   *index[2];            		/* atom_id's of the atoms inthe groups */
  t_filenm  fnm[] = {             		/* files for g_sgangle 	*/
    { efTRX, "-f", NULL,  ffREAD },    		/* trajectory file 	*/
    { efNDX, NULL, NULL,  ffREAD },    		/* index file 		*/
    { efTPX, NULL, NULL,  ffREAD },    		/* topology file 	*/
    { efXVG,"-oa","sg_angle",ffWRITE },		/* xvgr output file 	*/
    { efXVG, "-od","sg_dist",ffWRITE }, 	/* xvgr output file 	*/
    { efXVG, "-od1", "sg_dist1",ffWRITE }, 	/* xvgr output file 	*/
    { efXVG, "-od2", "sg_dist2",ffWRITE } 	/* xvgr output file 	*/
  };

#define NFILE asize(fnm)

  CopyRight(stderr,argv[0]);
  parse_common_args(&argc,argv,PCA_CAN_VIEW | PCA_CAN_TIME,TRUE,
		    NFILE,fnm,0,NULL,asize(desc),desc,0,NULL);
  
  init_lookup_table(stdout);

  top = read_top(ftp2fn(efTPX,NFILE,fnm));     /* read topology file */

  /* read index file. */
  rd_index(ftp2fn(efNDX,NFILE,fnm),2,gnx,index,grpname); 

  print_types(index[0],gnx[0],grpname[0],      /* show atomtypes, to check */
	     index[1],gnx[1],grpname[1],top);  /* if index file is correct */

  
  sgangle_plot(ftp2fn(efTRX,NFILE,fnm), 
	       opt2fn("-oa",NFILE,fnm), /* make plot */
	       opt2fn("-od",NFILE,fnm),
	       opt2fn("-od1",NFILE,fnm),
	       opt2fn("-od2",NFILE,fnm),
	       index[0],gnx[0],grpname[0],
	       index[1],gnx[1],grpname[1],
	       top);

  xvgr_file(opt2fn("-oa",NFILE,fnm),NULL);     /* view xvgr file */
  xvgr_file(opt2fn("-od",NFILE,fnm),NULL);     /* view xvgr file */
  xvgr_file(opt2fn("-od1",NFILE,fnm),NULL);
  xvgr_file(opt2fn("-od2",NFILE,fnm),NULL);

  thanx(stdout);
  return 0;
}










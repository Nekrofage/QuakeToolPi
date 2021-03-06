
#include "bsp5.h"

int		outleafs;

/*
===========
PointInLeaf
===========
*/
node_t	*PointInLeaf (node_t *node, vec3_t point)
{
	vec_t	d;
	
	if (node->contents)
		return node;
		
	d = DotProduct (planes[node->planenum].normal, point) - planes[node->planenum]. dist;
	
	if (d > 0)
		return PointInLeaf (node->children[0], point);
	
	return PointInLeaf (node->children[1], point);
}


void FloodEntDist_r(node_t *n,int dist)
{
   portal_t *p;
   int s;

   n->o_dist=dist;

   for (p=n->portals;p;p=p->next[s])
   {
      s=(p->nodes[1]==n);

//      if ((p->nodes[!s]->o_dist<=dist) && (p->nodes[!s]->o_dist))
      if (p->nodes[!s]->o_dist)
         continue;

      if ((p->nodes[0]->contents==CONTENTS_SOLID) ||
          (p->nodes[1]->contents==CONTENTS_SOLID))
         continue;

      if ((p->nodes[0]->contents==CONTENTS_SKY) ||
          (p->nodes[1]->contents==CONTENTS_SKY))
         continue;

      FloodEntDist_r(p->nodes[!s],dist+1);
   }
}

/*
===========
PlaceOccupant
===========
*/
qboolean PlaceOccupant (int num, vec3_t point, node_t *headnode)
{
	node_t	*n;
	
	n = PointInLeaf (headnode, point);
	if (n->contents == CONTENTS_SOLID)
		return false;
	n->occupied = num;

   FloodEntDist_r(n,1);

	return true;
}


/*
==============
MarkLeakTrail
==============
*/
portal_t	*prevleaknode;
FILE	*leakfile;
void MarkLeakTrail (portal_t *n2)
{
	int		i, j;
	vec3_t	p1, p2, dir;
	float	len;
	portal_t *n1;

	if (hullnum)
		return;

	n1 = prevleaknode;
	prevleaknode = n2;
	
	if (!n1)
		return;
		
	VectorCopy (n2->winding->points[0], p1);
	for (i=1 ; i< n2->winding->numpoints ; i++)
	{
		for (j=0 ; j<3 ; j++)
			p1[j] = (p1[j] + n2->winding->points[i][j]) / 2;
	}
	
	VectorCopy (n1->winding->points[0], p2);
	for (i=1 ; i< n1->winding->numpoints ; i++)
	{
		for (j=0 ; j<3 ; j++)
			p2[j] = (p2[j] + n1->winding->points[i][j]) / 2;
	}
		
	VectorSubtract (p2, p1, dir);
	len = VectorLength (dir);
	VectorNormalize (dir);
	
	while (len > 2)
	{
		fprintf (leakfile,"%f %f %f\n", p1[0], p1[1], p1[2]);
		for (i=0 ; i<3 ; i++)
			p1[i] += dir[i]*2;
		len -= 2;
	}
}

/*
==================
RecursiveFillOutside

If fill is false, just check, don't fill
Returns true if an occupied leaf is reached
==================
*/
int		hit_occupied;
int		backdraw;
qboolean RecursiveFillOutside (node_t *l, qboolean fill)
{
	portal_t	*p;
	int			s;

	if (l->contents == CONTENTS_SOLID || l->contents == CONTENTS_SKY)
		return false;
		
	if (l->valid == valid)
		return false;

	if (l->occupied)
   {
      hit_occupied=l->occupied;
		return true;
   }

	l->valid = valid;

// fill it and it's neighbors
	if (fill)
		l->contents = CONTENTS_SOLID;
	outleafs++;

	for (p=l->portals ; p ; )
	{
		s = (p->nodes[0] == l);

		if (RecursiveFillOutside (p->nodes[s], fill) )
		{	// leaked, so stop filling
/*			if (backdraw-- > 0)  // replaced by new, smarter, Q2 style code
			{				
				MarkLeakTrail (p);
				DrawLeaf (l, 2);
			}*/
			return true;
		}
		p = p->next[!s];
	}

	return false;
}

/*
==================
ClearOutFaces

==================
*/
void ClearOutFaces (node_t *node)
{
	face_t	**fp;
	
	if (node->planenum != -1)
	{
		ClearOutFaces (node->children[0]);
		ClearOutFaces (node->children[1]);
		return;
	}
	if (node->contents != CONTENTS_SOLID)
		return;

	for (fp=node->markfaces ; *fp ; fp++)
	{
	// mark all the original faces that are removed
		(*fp)->numpoints = 0;
	}
	node->faces = NULL;
}


//=============================================================================

/*
===========
FillOutside

===========
*/
qboolean FillOutside (node_t *node)
{
	int			s;
	vec_t		*v;
	int			i;
	qboolean	inside;
	
	qprintf ("----- FillOutside ----\n");

	if (nofill)
	{
		printf ("skipped\n");
		return false;
	}
		
	inside = false;
	for (i=1 ; i<num_entities ; i++)
	{
		if (!VectorCompare(entities[i].origin, vec3_origin))
		{
         if (!strcmp(ValueForKey(&entities[i],"classname"),
             "info_player_start")) // make sure all hulls can be filled
         {
			   if (!PlaceOccupant (i, entities[i].origin, node))
            {
               entities[i].origin[2]+=16;
   			   if (PlaceOccupant (i, entities[i].origin, node))
   				   inside = true;
               entities[i].origin[2]-=16;
            }
            else
            {
               inside = true;
            }
         }
         else
			{
			   if (PlaceOccupant (i, entities[i].origin, node))
				   inside = true;
         }
		}
	}

	if (!inside)
	{
		printf ("Hullnum %i: No entities in empty space -- no filling performed\n", hullnum);
		return false;
	}

	s = !(outside_node.portals->nodes[1] == &outside_node);

// first check to see if an occupied leaf is hit
	outleafs = 0;
	valid++;

	prevleaknode = NULL;
	
	if (!hullnum)
	{
		leakfile = fopen (pointfilename, "w");
		if (!leakfile)
			Error ("Couldn't open %s\n", pointfilename);
	}

	if (RecursiveFillOutside (outside_node.portals->nodes[s], false))
	{
		v = entities[hit_occupied].origin;
		qprintf ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		qprintf ("reached occupant at: (%4.0f,%4.0f,%4.0f)\n",v[0],v[1],v[2]);
      qprintf ("classname: '%s'\n",ValueForKey(&entities[hit_occupied],"classname"));
		qprintf ("no filling performed\n");
		qprintf ("leak file written to %s\n", pointfilename);			
		qprintf ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

      if (!hullnum)
      {
         FILE *f;
         node_t *n;
         portal_t *p,*p2;
         int side;
         int next;
         node_t *nextnode;
         vec3_t wc;
         int i,j;

         f=leakfile;

         n=&outside_node;
         next=-1;

         while ((n->o_dist>1) || (next==-1))
         {
            nextnode=NULL;
            p2=NULL;
            for (p=n->portals;p;p=p->next[side])
            {
               side=(p->nodes[1]==n);
               if ((next==-1) ||
                   ((p->nodes[!side]->o_dist<next) &&
                    (p->nodes[!side]->o_dist))
                  )
               {
                  next=p->nodes[!side]->o_dist;
                  nextnode=p->nodes[!side];
                  p2=p;
               }
            }
            if (nextnode)
            {
               n=nextnode;
   
               wc[0]=wc[1]=wc[2]=0;
               for (i=0;i<p2->winding->numpoints;i++)
               {
                  for (j=0;j<3;j++)
                     wc[j]+=p2->winding->points[i][j];
               }
               for (j=0;j<3;j++)
                  wc[j]/=i;
               fprintf(f,"%g %g %g\n",wc[0],wc[1],wc[2]);
            }
            else
               break;
         }
		   v = entities[n->occupied].origin;
		   fprintf(f,"%g %g %g\n",v[0],v[1],v[2]);

			fclose (leakfile);
      }

		return false;
	}
	if (!hullnum)
		fclose (leakfile);

// now go back and fill things in
	valid++;
	RecursiveFillOutside (outside_node.portals->nodes[s], true);

// remove faces from filled in leafs	
	ClearOutFaces (node);
	
	qprintf ("%4i outleafs\n", outleafs);
	return true;
}



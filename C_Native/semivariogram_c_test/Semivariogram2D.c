/*
 * Semivariogram2D.c
 *
 *  Created on: Feb 6, 2014
 *      Author: Mata
 */

#include <stdio.h>
#include <stdlib.h>
#include "Semivariogram2D.h"
//#define  DEBUG_SEMIVARIOGRAM_LEVEL1
//#define  DEBUG_SEMIVARIOGRAM_LEVEL2
//#include "DataStruct.h"

void calculateSemivariogram(char* inputPathLIDARdata, double *sum_SqurZ, double *distance, int nrbins ){
	printf("Calculate Semivariogram ...\n");

	//This variables is Semivariogram settings
	//int nrbins = 50;

	//Read input file and put in into Array
	double atof(const char* str);
	char line[100];
	char lidarxyz[30];
	int counter_line=0;


	FILE *file;
	file=fopen(inputPathLIDARdata,"r");
	if(file==NULL){
		fprintf(stderr,"[Semivariogram2D.c] cannot open input LIDAR Data\n");
		exit(EXIT_FAILURE);
	}

	//create struct array list
	struct LIDARnode *new_node, *current, *listLIDARnodes=NULL;

	//check each line and parse it into listLIDARnodes
	while(fgets(line,sizeof line, file)!=NULL){
		//parse this line into X,Y,Z
		const char *ptr = line;
		int n;
		int counter = 0;

		while ( sscanf(ptr, "%31[^ ]%n", lidarxyz, &n) == 1 )
		{
			DEBUG_PRINT2("lidarxyz = \"%s\"\n", lidarxyz);
			double x = atof(lidarxyz);


			switch(counter){
			case 0:
				new_node = (struct LIDARnode*) malloc(sizeof(struct LIDARnode));
				new_node->X = x;

				break;
			case 1:
				new_node->Y = x;
				break;
			case 2:
				new_node->Z = x;
				new_node->next = NULL;

				if(listLIDARnodes==NULL){
					listLIDARnodes = new_node;
					current	= new_node;
				}else{
					current->next = new_node;
					current= new_node;
				}

				break;

			default:
				fprintf(stderr, "[Semivariogram2D.c] error in counter\n");
			}

			counter++;
			ptr += n; /* advance the pointer by the number of characters read */
			if ( *ptr != ' ' )
			{
				break; /* didn't find an expected delimiter, done? */
			}
			++ptr; /* skip the delimiter */
		}
		counter_line++;
	}

	//check value of listLIDARnodes
	struct LIDARnode *check_nodes;
	check_nodes = listLIDARnodes;
	while(check_nodes!=NULL){
		DEBUG_PRINT1("<X,Y,Z> = <%f,%f,%f>\n",check_nodes->X,check_nodes->Y,check_nodes->Z);
		check_nodes=check_nodes->next;
	}

	// Find min & max value
	double minY=listLIDARnodes->Y;
	double minX=listLIDARnodes->X;
	double maxY=listLIDARnodes->Y;
	double maxX=listLIDARnodes->X;
	check_nodes = listLIDARnodes;
	while(check_nodes!=NULL){
		if(minY>check_nodes->Y)
			minY = check_nodes->Y;
		if(minX>check_nodes->X)
			minX = check_nodes->X;
		if(maxX<check_nodes->X)
			maxX=check_nodes->X;
		if(maxY<check_nodes->Y)
			maxY=check_nodes->Y;
		check_nodes=check_nodes->next;
	}
	DEBUG_PRINT1("<X,Y>max = <%f,%f>\n",maxX,maxY);
	DEBUG_PRINT1("<X,Y>min = <%f,%f>\n",minX,minY);

	//calculate max distance & delta
	double maxDistance = sqrt(pow(maxX-minX,2)+pow(maxY-minY,2))/2;
	double delta = maxDistance/nrbins;
	DEBUG_PRINT2("maxDistance=%f, delta=%f\n",maxDistance,delta);

	//create index distBins
	double *distBins = (double*)malloc(sizeof(double)*(nrbins+2));
	double *occurance_idx_distBins = (double*)malloc(sizeof(double)*(nrbins+2));
	memset(occurance_idx_distBins,0,sizeof(occurance_idx_distBins));
	distBins[0]=0.0;
	DEBUG_PRINT2("nrbins:%d , distBins:%lf\n", 0, distBins[0]);
	int k;
	for (k=1 ; k<nrbins ; k++){
		distBins[k] = distBins[k-1] + delta;
		DEBUG_PRINT2("nrbins:%d , distBins:%lf\n", k, distBins[k]);
	}
	distBins [nrbins] = maxDistance;
	DEBUG_PRINT2("nrbins:%d , distBins:%lf\n", nrbins, distBins[nrbins]);

	/*calculate pair distance for every nodes and Finding correlated Index distBin
	 and save the result into new data struct LIDARPairDistanceNode
	 */
	struct LIDARnode *ptr1, *ptr2,*to_be_free;
	ptr1 = listLIDARnodes;

	//calculate sum of total squrl for each idx_distbins which is has equal value
	//double sum_SqurZ[nrbins+1];
	memset(sum_SqurZ,0,sizeof(sum_SqurZ));
	while(ptr1){
		ptr2 = ptr1->next;
		while(ptr2){
			//calculate distance between ptr1 & ptr2
			double distance = sqrt(pow(ptr1->X-ptr2->X,2)+ pow(ptr1->Y-ptr2->Y,2));
			DEBUG_PRINT1("distance: %f\n", distance);
			//index to distBins if distance is less than maxDistance
			if(distance<maxDistance){

				int idx_distbins = floor(distance/delta);
				DEBUG_PRINT1("range distbins %f - ",distBins[idx_distbins]);
				occurance_idx_distBins[idx_distbins] =occurance_idx_distBins[idx_distbins]+1;
				DEBUG_PRINT1("%f \n",distBins[idx_distbins+1]);

				//calculate SqurZ
				double squrl = pow(ptr1->Z-ptr2->Z,2);
				sum_SqurZ[idx_distbins] = sum_SqurZ[idx_distbins]+squrl;


			}
			ptr2=ptr2->next;
		}
		to_be_free = ptr1;
		ptr1=ptr1->next;
		free(to_be_free);
	}

	//check sum of total squrl for each idx_distbins which is has equal value
	for(k=0;k<nrbins;k++){
		DEBUG_PRINT2("sum_SqurZ[%d]: %f\n",k,sum_SqurZ[k]);
		DEBUG_PRINT2("occurance_idx_distBins[%d] : %d\n",k,occurance_idx_distBins[k]);
	}


	// total sum SqurZ divide by 2*occurance
	for(k=0;k<nrbins;k++){
		if(sum_SqurZ[k] != 0){
			sum_SqurZ[k] = sum_SqurZ[k]/(2*occurance_idx_distBins[k]);
		}

		printf("%d : %1f\n",k, sum_SqurZ[k]);
	}
	// Final variable is sum_SqurZ

	for (k=0;k<nrbins;k++){
		distance[k] = distBins[k] + delta/2;
	}	
	printf("Finished Calculate Semivariogram\n");


	
	return;
}


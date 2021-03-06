#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <malloc.h>
#include <string.h>

#include "gridding.h"

#define DEBUGLEVEL0
//#define DEBUGLEVEL1

//debug mode enable printf
#ifdef DEBUGLEVEL0
#define PRINTDEBUGMODE0 printf
#else
#define PRINTDEBUGMODE0(format, args...) ((void)0)
#endif

//this debug macro is for more details
#ifdef DEBUGLEVEL1
#define PRINTDEBUGMODE1 printf
#else
#define PRINTDEBUGMODE1(format, args...) ((void)0)
#endif

#define NUM_MAX_PROCESS 12
#define GRID_SIZE 1
#define BUF_SIZE 200000

void findMinMax();
char* itoa();
void CalDistPointfromGrid();
double Eculidean();

double minX=0.0, minY=0.0, maxX=0.0, maxY=0.0;
//Lidar Data mapped grid
//Info_Grid *startGrid=NULL;
PointerOfGrid **buffNodes_inOneGrid=NULL;
Info_Grid *new_list=NULL;
Info_Grid *cur=NULL;
Info_Grid *forFreeGrid=NULL;

//Calculate distance from grid point
Distnode *Head_distfromGrid=NULL;
Distnode *Cur_DistfromGrid=NULL;
Distnode *Tail_DistfromGrid=NULL;
Distnode *ForFree_DistfromGrid=NULL;

//Linked list selection sort
Distnode *Cur_Sort=NULL;
Distnode *PrevCur_Sort=NULL;
Distnode *Pivot_Sort=NULL;
Distnode *PrevPivot_Sort=NULL;
Distnode *tmp_Sort=NULL;

int main(int argc, char *argv[]){

	int rankId, numProcess;
	int bufflen = 512;
	char hostname[bufflen];

	int i, j, k, p;
	int dsize=0, quotient=0, extra=0, avgrows=0;
	int nrbins = 50, nrSubbins = 100, sameIdxCnt;
	double *x, *y, *z, *t, *distBins, **ptopDist;
	//double maxD;
	int minX_inputData, minY_inputData, maxX_inputData, maxY_inputData, gridXrange, gridYrange, gridXsize, gridYsize;
	int numberofGrids_X, numberofGrids_Y;
	double gridX, gridY;
	int startX, startY, endX, endY, XofGrid, YofGrid;
	int NNendX, NNendY;
	int alphaGridX, alphaGridY;
	int radX, radY, idxOfRadius=0, searchRadius=3, numOfNearestPoint=10;//for radius
	int cnt_point=0, cardinal_direction;//0:x++ 1:y++ 2:x-- 3:y--
	int divYsize, totlaPoints=0;
	double delta, sumZ, maxDistance;
	double startTime, endTime, stGtime, endGtime, stSemitime, endSemitime, stFitime, endFitime, stPtime, endPtime;
	double totalGtime=0.0, totalSemitime=0.0, totalFitime=0.0, totalPtime=0.0, totalTime=0.0;
	double maxTime=0.0, maxGtime=0.0, maxSemitime=0.0, maxFitime=0.0, maxPtime=0.0;
	double Gridsqure, progrssCnt=0;

	double buffer_grid[BUF_SIZE][3];

	//For variogram
	double sum_SqurZ[nrbins+2], distDelta[nrbins+2];

	char charRank[4];
	char path[50]="/home/mpiuser/Documents/PointInGrid";
	char path_p[50]="/nfs/code/ksy/output/PredictionPerGrid";
	char fileType[] = ".txt";
	FILE *fpdata=NULL, *ferr=NULL, *file_out=NULL;
	FILE *fPrediction=NULL;

	struct Info_vario *Info_v;

	int msg=0;

	MPI_Request req;
	MPI_Status status;
	MPI_File thefile;
	MPI_Offset fileOffset;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rankId);
	MPI_Comm_size(MPI_COMM_WORLD, &numProcess);
	if(numProcess >= NUM_MAX_PROCESS){
		printf("[ERROR] numProcess > NUM_MAX_PROCESS\n");
		exit(0);
	}

	startTime=MPI_Wtime();
	static const char filename[] = "/nfs/code/ksy/data/Data3_XYZ_Ground.txt";

	/*===================== Read Data =============================*/
	FILE *file = fopen ( filename, "r" );
	int inputsize=0;
	if ( file != NULL )
	{
		char line [ 128 ]; /* or other suitable maximum line size */
		while ( fgets ( line, sizeof line, file ) != NULL ) /* read a line */
		{
			//fputs ( line, stdout ); /* write the line */
			inputsize++;
		}
		fclose ( file );
	}
	else
	{
		perror ( filename ); /* why didn't the file open? */
	}

	dsize = inputsize;
	i=0; 

	fpdata = fopen(filename,"r");
	x = (double*)malloc(sizeof(double)*dsize);
	y = (double*)malloc(sizeof(double)*dsize);
	z = (double*)malloc(sizeof(double)*dsize);

	fseek(fpdata,0,SEEK_SET);
	gethostname(hostname, bufflen);
	PRINTDEBUGMODE1( "Start process %d at %s\n", rankId, hostname );

	//Preprocessing steps is here
	while(!feof(fpdata)){
		fscanf(fpdata, "%lf %lf %lf", &x[i], &y[i], &z[i]);
		findMinMax(x[i], y[i]);
		//fprintf(ferr,"data: %lf %lf %lf\n", x[i], y[i], z[i]);
		i++;
	}
	minX_inputData = (int)floor(minX);	maxX_inputData = (int)ceil(maxX);	minY_inputData = (int)floor(minY);	maxY_inputData = (int)ceil(maxY);
	gridXrange = maxX_inputData-minX_inputData; gridYrange = maxY_inputData-minY_inputData;
	if((((int)gridXrange % GRID_SIZE)!=0 )|(((int)gridYrange % GRID_SIZE)!=0)){
		printf("[ERROR] (gridXrange % GRID_SIZE)!=0 )|((gridYrange / GRID_SIZE)!=0)\n");
		exit(0);
	}
	numberofGrids_X = gridXrange / GRID_SIZE; numberofGrids_Y = gridYrange / GRID_SIZE;
	int totalGrids = numberofGrids_X * numberofGrids_Y;
	int numGridPerNode = ceil(totalGrids/numProcess);
	int startGrid_X[NUM_MAX_PROCESS];
	int startGrid_Y[NUM_MAX_PROCESS];
	// we create Grid Index per process
	int grid_index[NUM_MAX_PROCESS];
	// specify grid index for each process
	grid_index[rankId] = rankId*numGridPerNode;

	//we map grid_index to X,Y point to be used in the start of iteration per processor
	startGrid_X[rankId] = (int)(grid_index[rankId]%numberofGrids_X);
	startGrid_Y[rankId] = (int)floor(grid_index[rankId]/numberofGrids_X);

	if(rankId==0) {
		PRINTDEBUGMODE0( "Total process %d\n", numProcess);
		PRINTDEBUGMODE0(" min X: %d; max X: %d \n min Y: %d max Y: %d\n", minX_inputData, maxX_inputData, minY_inputData, maxY_inputData);
		PRINTDEBUGMODE0("GRID_SIZE : %2.2f meter\n",(double)GRID_SIZE);
		PRINTDEBUGMODE0("GRIDrange X:%d; GRIDrange Y:%d\n", gridXrange, gridYrange);
		PRINTDEBUGMODE0("numberofGrids_X:%d; numberofGrids_Y:%d\n", numberofGrids_X, numberofGrids_Y);
		PRINTDEBUGMODE0("totalGrids:%d\n",totalGrids);
		PRINTDEBUGMODE0("numGridsPerNode:%d\n",numGridPerNode);
	}
	PRINTDEBUGMODE0("grid_index[%d]:%d\n",rankId,grid_index[rankId]);
	PRINTDEBUGMODE1("startGrid[%d]:(%2.2f,%2.2f)\n",rankId,startGrid_X[rankId],startGrid_Y[rankId]);

	// Prepare buffer to store nodes in a grid within searchRange
	buffNodes_inOneGrid = (PointerOfGrid**)malloc(sizeof(PointerOfGrid*)*(numberofGrids_X));

	PRINTDEBUGMODE1("malloc buffNodes_inOneGrid %d ...\n", rankId);
	for(i=0;i<numberofGrids_X;i++){
		buffNodes_inOneGrid[i] = (PointerOfGrid*)malloc(sizeof(PointerOfGrid)*numberofGrids_Y);
		for(j=0;j<numberofGrids_Y;j++){
			buffNodes_inOneGrid[i][j].next=NULL;
		}
		//FOR_DEBUG_PRINT("row_num:%d\n",i);FOR_DEBUG_PRINT("Load grid on memory\n");
	}

	// lets process data input one by one
	int grid_idx;
	for(i=0;i<dsize;i++){
		double temp_gridX = (int)(x[i]-minX_inputData)/GRID_SIZE;
		double temp_gridY = (int)(y[i]-minY_inputData)/GRID_SIZE;

		for(grid_idx=0;grid_idx<numGridPerNode;grid_idx++){
			//Find X,Y grid points
			gridX = minX_inputData+((startGrid_X[rankId]+grid_idx*GRID_SIZE)%numberofGrids_X);
			gridY = minY_inputData+startGrid_Y[rankId]+ceil((startGrid_X[rankId]+(grid_idx*GRID_SIZE))/numberofGrids_X);

			double xrange = gridX-x[i];
			if(xrange<0) xrange=-xrange;
			double yrange = gridY-y[i];
			if(yrange<0) yrange=-yrange;

			//check whether input point is located within searchRange or not
			if((xrange<searchRadius)&&(yrange<searchRadius)){
				//lets store to buffer
				cur = buffNodes_inOneGrid[(int)((int)(gridX-minX_inputData)/GRID_SIZE)][(int)((int)(gridY-minY_inputData)/GRID_SIZE)].next;

				if(cur==NULL){
					PRINTDEBUGMODE1("cur next NULL\n");
					new_list = (Info_Grid*)malloc(sizeof(Info_Grid));
					new_list->x = x[i];
					new_list->y = y[i];
					new_list->z = z[i];
					new_list->next=NULL;

					buffNodes_inOneGrid[((int)(gridX-minX_inputData)/GRID_SIZE)][((int)(gridY-minY_inputData)/GRID_SIZE)].next = new_list;
				} else {
					PRINTDEBUGMODE1("cur next NOT NULL\n");
					while(cur->next!=NULL) {
						cur = cur->next;
					}

					new_list = (Info_Grid*)malloc(sizeof(Info_Grid));
					new_list->x = x[i];
					new_list->y = y[i];
					new_list->z = z[i];

					new_list->next=NULL;
					cur->next = new_list;
				}
			}
			PRINTDEBUGMODE1("HORE\n");

		}
		PRINTDEBUGMODE1("BERHASIL\n");
	}
	free(x);
	free(y);
	free(z);

	// TODO ngelanjutin
	endTime = MPI_Wtime();
	totalTime = endTime-startTime;
	MPI_Finalize();
	PRINTDEBUGMODE0( "Finished process %d at %s in %lf seconds\n", rankId, hostname, totalTime );

	return 0;

}

void findMinMax (double x, double y){
	if (minX == 0.0 && maxX == 0.0){
		minX = x;
		minY = y;
		maxX = x;
		maxY = y;
	}

	if(minY>y)
		minY = y;
	if(minX>x)
		minX = x;
	if(maxX<x)
		maxX = x;
	if(maxY<y)
		maxY = y;
}

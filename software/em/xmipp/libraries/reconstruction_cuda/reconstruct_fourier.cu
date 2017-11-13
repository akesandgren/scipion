#if SHARED_BLOB_TABLE
__shared__ float BLOB_TABLE[BLOB_TABLE_SIZE_SQRT];
#endif

#if SHARED_IMG
__shared__ Point3D<float> SHARED_AABB[2];
extern __shared__ float2 IMG[];
#endif

// FIELDS

// Holding streams used for calculation. Present on CPU
cudaStream_t* streams;

// Wrapper to hold pointers to GPU memory (and have it also accessible from CPU)
std::map<int,FRecBufferDataGPUWrapper*> wrappers;

// Holding blob coefficient table. Present on GPU
float* devBlobTableSqrt = NULL;

__device__ __constant__ int cMaxVolumeIndexX = 0;
__device__ __constant__ int cMaxVolumeIndexYZ = 0;
__device__ __constant__ float cBlobRadius = 0.f;
__device__ __constant__ float cBlobAlpha = 0.f;
__device__ __constant__ float cIw0 = 0.f;
__device__ __constant__ float cIDeltaSqrt = 0.f;


__device__
float bessi0(float x)
{
    float y, ax, ans;
    if ((ax = fabsf(x)) < 3.75f)
    {
        y = x / 3.75f;
        y *= y;
        ans = 1.f + y * (3.5156229f + y * (3.0899424f + y * (1.2067492f
                                          + y * (0.2659732f + y * (0.360768e-1f + y * 0.45813e-2f)))));
    }
    else
    {
        y = 3.75f / ax;
        ans = (expf(ax) / sqrtf(ax)) * (0.39894228f + y * (0.1328592e-1f
                                      + y * (0.225319e-2f + y * (-0.157565e-2f + y * (0.916281e-2f
                                                                + y * (-0.2057706e-1f + y * (0.2635537e-1f + y * (-0.1647633e-1f
                                                                                            + y * 0.392377e-2f))))))));
    }
    return ans;
}

__device__
float bessi1(float x)
{
    float ax, ans;
    float y;
    if ((ax = fabsf(x)) < 3.75f)
    {
        y = x / 3.75f;
        y *= y;
        ans = ax * (0.5f + y * (0.87890594f + y * (0.51498869f + y * (0.15084934f
                               + y * (0.2658733e-1f + y * (0.301532e-2f + y * 0.32411e-3f))))));
    }
    else
    {
        y = 3.75f / ax;
        ans = 0.2282967e-1f + y * (-0.2895312e-1f + y * (0.1787654e-1f
                                  - y * 0.420059e-2f));
        ans = 0.39894228f + y * (-0.3988024e-1f + y * (-0.362018e-2f
                                + y * (0.163801e-2f + y * (-0.1031555e-1f + y * ans))));
        ans *= (expf(ax) / sqrtf(ax));
    }
    return x < 0.0 ? -ans : ans;
}

__device__
float bessi2(float x)
{
    return (x == 0) ? 0 : bessi0(x) - ((2*1) / x) * bessi1(x);
}

__device__
float bessi3(float x)
{
    return (x == 0) ? 0 : bessi1(x) - ((2*2) / x) * bessi2(x);
}

__device__
float bessi4(float x)
{
    return (x == 0) ? 0 : bessi2(x) - ((2*3) / x) * bessi3(x);
}


template<int order>
__device__
float kaiserValue(float r, float a, float alpha)
{
    float rda, rdas, arg, w;

    rda = r / a;
    if (rda <= 1.f)
    {
        rdas = rda * rda;
        arg = alpha * sqrtf(1.f - rdas);
        if (order == 0)
        {
            w = bessi0(arg) / bessi0(alpha);
        }
        else if (order == 1)
        {
            w = sqrtf (1.f - rdas);
            if (alpha != 0.f)
                w *= bessi1(arg) / bessi1(alpha);
        }
        else if (order == 2)
        {
            w = sqrtf (1.f - rdas);
            w = w * w;
            if (alpha != 0.f)
                w *= bessi2(arg) / bessi2(alpha);
        }
        else if (order == 3)
        {
            w = sqrtf (1.f - rdas);
            w = w * w * w;
            if (alpha != 0.f)
                w *= bessi3(arg) / bessi3(alpha);
        }
        else if (order == 4)
        {
            w = sqrtf (1.f - rdas);
            w = w * w * w *w;
            if (alpha != 0.f)
                w *= bessi4(arg) / bessi4(alpha);
        }
        else {
        	printf("order (%d) out of range in kaiser_value(): %s, %d\n", order, __FILE__, __LINE__);
        }
    }
    else
        w = 0.f;

    return w;
}




/** Returns true if x is in (min, max), i.e. opened, interval */
template <typename T>
__device__
static bool inRange(T x, T min, T max) {
	return (x > min) && (x < max);
}


/** Returns value within the range (included) */
template<typename T, typename U>
__device__
static U clamp(U val, T min, T max) {
	U res = (val > max) ? max : val;
	return (res < min) ? min : res;
}

__device__
const float* getNthItem(const float* array, int itemIndex, int fftSizeX, int fftSizeY, bool isFFT) {
//	if (array == FFTs)
		return array + (fftSizeX * fftSizeY * itemIndex * (isFFT ? 2 : 1)); // *2 since it's complex
//	if (array == CTFs) return array + (fftSizeX * fftSizeY * itemIndex);
//	if (array == modulators) return array + (fftSizeX * fftSizeY * itemIndex);
//	if (array == paddedImages) return array + (paddedImgSize * paddedImgSize * itemIndex);
//	return NULL; // undefined
}

/**
 * Calculates Z coordinate of the point [x, y] on the plane defined by p0 (origin) and two vectors
 * Returns 'true' if the in point lies within parallelogram
 */
__device__
bool getZ(float x, float y, float& z, const RecFourierProjectionTraverseSpace* space, float p0X, float p0Y, float p0Z) {
	// from parametric eq. of the plane
	float u = ((y-p0Y)*space->uX + (p0X-x)*space->uY) / (space->uX * space->vY - space->vX * space->uY);
	float t = (-p0X + x - u*space->vX) / (space->uX);

	z = p0Z + t*space->uZ + u*space->vZ;
	return inRange(t, 0.f, 1.f) && inRange(u, 0.f, 1.f);
}

/**
 * Calculates Y coordinate of the point [x, z] on the plane defined by p0 (origin) and two vectors
 * Returns 'true' if the in point lies within parallelogram
 */
__device__
bool getY(float x, float& y, float z, const RecFourierProjectionTraverseSpace* space, float p0X, float p0Y, float p0Z) {
	// from parametric eq. of the plane
	float u = ((z-p0Z)*space->uX + (p0X-x)*space->uZ) / (space->uX * space->vZ - space->vX * space->uZ);
	float t = (-p0X + x - u*space->vX) / (space->uX);

	y = p0Y + t*space->uY + u*space->vY;
	return inRange(t, 0.f, 1.f) && inRange(u, 0.f, 1.f);
}

/**
 * Calculates X coordinate of the point [y, z] on the plane defined by p0 (origin) and two vectors
 * Returns 'true' if the in point lies within parallelogram
 */
__device__
bool getX(float& x, float y, float z, const RecFourierProjectionTraverseSpace* space, float p0X, float p0Y, float p0Z) {
	// from parametric eq. of the plane
	float u = ((z-p0Z)*space->uY + (p0Y-y)*space->uZ) / (space->uY * space->vZ - space->vY * space->uZ);
	float t = (-p0Y + y - u*space->vY) / (space->uY);

	x = p0X + t*space->uX + u*space->vX;
	return inRange(t, 0.f, 1.f) && inRange(u, 0.f, 1.f);
}

/** Do 3x3 x 1x3 matrix-vector multiplication */
__device__
void multiply(float t00, float t01, float t02, float t10, float t11, float t12, float t20, float t21, float t22, Point3D<float>& inOut) {
	float tmp0 = t00 * inOut.x + t01 * inOut.y + t02 * inOut.z;
	float tmp1 = t10 * inOut.x + t11 * inOut.y + t12 * inOut.z;
	float tmp2 = t20 * inOut.x + t21 * inOut.y + t22 * inOut.z;
	inOut.x = tmp0;
	inOut.y = tmp1;
	inOut.z = tmp2;
}

/** Do 3x3 x 1x3 matrix-vector multiplication */
__device__
void multiply(const RecFourierProjectionTraverseSpace* tSpace, Point3D<float>& inOut) {
	multiply(tSpace->transformInv00, tSpace->transformInv01, tSpace->transformInv02,
			tSpace->transformInv10, tSpace->transformInv11, tSpace->transformInv12,
			tSpace->transformInv20, tSpace->transformInv21, tSpace->transformInv22, inOut);
}

/**
 * Method will rotate box using transformation matrix around center of the
 * working space
 */
__device__
void rotate(Point3D<float>* box, const RecFourierProjectionTraverseSpace* tSpace) {
	for (int i = 0; i < 8; i++) {
		Point3D<float> imgPos;
		// transform current point to center
		imgPos.x = box[i].x - cMaxVolumeIndexX/2;
		imgPos.y = box[i].y - cMaxVolumeIndexYZ/2;
		imgPos.z = box[i].z - cMaxVolumeIndexYZ/2;
		// rotate around center
		multiply(tSpace, imgPos);
		// transform back just Y coordinate, since X now matches to picture and Z is irrelevant
		imgPos.y += cMaxVolumeIndexYZ / 2;

		box[i] = imgPos;
	}
}

/** Compute Axis Aligned Bounding Box of given cuboid */
__device__
void computeAABB(Point3D<float>* AABB, Point3D<float>* cuboid) {
	AABB[0].x = AABB[0].y = AABB[0].z = INFINITY;
	AABB[1].x = AABB[1].y = AABB[1].z = -INFINITY;
	Point3D<float> tmp;
	for (int i = 0; i < 8; i++) {
		tmp = cuboid[i];
		if (AABB[0].x > tmp.x) AABB[0].x = tmp.x;
		if (AABB[0].y > tmp.y) AABB[0].y = tmp.y;
		if (AABB[0].z > tmp.z) AABB[0].z = tmp.z;
		if (AABB[1].x < tmp.x) AABB[1].x = tmp.x;
		if (AABB[1].y < tmp.y) AABB[1].y = tmp.y;
		if (AABB[1].z < tmp.z) AABB[1].z = tmp.z;
	}
	AABB[0].x = ceilf(AABB[0].x);
	AABB[0].y = ceilf(AABB[0].y);
	AABB[0].z = ceilf(AABB[0].z);

	AABB[1].x = floorf(AABB[1].x);
	AABB[1].y = floorf(AABB[1].y);
	AABB[1].z = floorf(AABB[1].z);
}

/**
 * Method will map one voxel from the temporal
 * spaces to the given projection and update temporal spaces
 * using the pixel value of the projection.
 */
template<bool hasCTF>
__device__
void processVoxel(
	float* tempVolumeGPU, float* tempWeightsGPU,
	const float* FFTs, const float* CTFs, const float* modulators,
	int fftSizeX, int fftSizeY,
	int x, int y, int z,
	const RecFourierProjectionTraverseSpace* const space)
{
	Point3D<float> imgPos;
	float wBlob = 1.f;
	float wCTF = 1.f;
	float wModulator = 1.f;
	const float* __restrict__ img = getNthItem(FFTs, space->projectionIndex, fftSizeX, fftSizeY, true);


	float dataWeight = space->weight;

	// transform current point to center
	imgPos.x = x - cMaxVolumeIndexX/2;
	imgPos.y = y - cMaxVolumeIndexYZ/2;
	imgPos.z = z - cMaxVolumeIndexYZ/2;
	if (imgPos.x*imgPos.x + imgPos.y*imgPos.y + imgPos.z*imgPos.z > space->maxDistanceSqr) {
		return; // discard iterations that would access pixel with too high frequency
	}
	// rotate around center
	multiply(space, imgPos);
	// transform back and round
	// just Y coordinate needs adjusting, since X now matches to picture and Z is irrelevant
	int imgX = clamp((int)(imgPos.x + 0.5f), 0, fftSizeX - 1);
	int imgY = clamp((int)(imgPos.y + 0.5f + cMaxVolumeIndexYZ / 2), 0, fftSizeY - 1);

	int index3D = z * (cMaxVolumeIndexYZ+1) * (cMaxVolumeIndexX+1) + y * (cMaxVolumeIndexX+1) + x;
	int index2D = imgY * fftSizeX + imgX;

	if (hasCTF) {
		const float* __restrict__ CTF = getNthItem(CTFs, space->projectionIndex, fftSizeX, fftSizeY, false);
		const float* __restrict__ modulator = getNthItem(modulators, space->projectionIndex, fftSizeX, fftSizeY, false);
		wCTF = CTF[index2D];
		wModulator = modulator[index2D];
	}

	float weight = wBlob * wModulator * dataWeight;

	 // use atomic as two blocks can write to same voxel
	atomicAdd(&tempVolumeGPU[2*index3D], img[2*index2D] * weight * wCTF);
	atomicAdd(&tempVolumeGPU[2*index3D + 1], img[2*index2D + 1] * weight * wCTF);
	atomicAdd(&tempWeightsGPU[index3D], weight);
}

/**
 * Method will map one voxel from the temporal
 * spaces to the given projection and update temporal spaces
 * using the pixel values of the projection withing the blob distance.
 */
template<bool hasCTF, int blobOrder>
__device__
void processVoxelBlob(
	float* tempVolumeGPU, float *tempWeightsGPU,
	const float* FFTs, const float* CTFs, const float* modulators,
	int fftSizeX, int fftSizeY,
	int x, int y, int z,
	const RecFourierProjectionTraverseSpace* const space,
	const float* blobTableSqrt,
	int imgCacheDim)
{
	Point3D<float> imgPos;
	// transform current point to center
	imgPos.x = x - cMaxVolumeIndexX/2;
	imgPos.y = y - cMaxVolumeIndexYZ/2;
	imgPos.z = z - cMaxVolumeIndexYZ/2;
	if ((imgPos.x*imgPos.x + imgPos.y*imgPos.y + imgPos.z*imgPos.z) > space->maxDistanceSqr) {
		return; // discard iterations that would access pixel with too high frequency
	}
	// rotate around center
	multiply(space, imgPos);
	// transform back just Y coordinate, since X now matches to picture and Z is irrelevant
	imgPos.y += cMaxVolumeIndexYZ / 2;

	// check that we don't want to collect data from far far away ...
	float radiusSqr = cBlobRadius * cBlobRadius;
	float zSqr = imgPos.z * imgPos.z;
	if (zSqr > radiusSqr) return;

	// create blob bounding box
	int minX = ceilf(imgPos.x - cBlobRadius);
	int maxX = floorf(imgPos.x + cBlobRadius);
	int minY = ceilf(imgPos.y - cBlobRadius);
	int maxY = floorf(imgPos.y + cBlobRadius);
	minX = fmaxf(minX, 0);
	minY = fmaxf(minY, 0);
	maxX = fminf(maxX, fftSizeX-1);
	maxY = fminf(maxY, fftSizeY-1);

	int index3D = z * (cMaxVolumeIndexYZ+1) * (cMaxVolumeIndexX+1) + y * (cMaxVolumeIndexX+1) + x;
	float volReal, volImag, w;
	volReal = volImag = w = 0.f;
#if !SHARED_IMG
	const float* __restrict__ img = getNthItem(FFTs, space->projectionIndex, fftSizeX, fftSizeY, true);
#endif
	float dataWeight = space->weight;

	// ugly spaghetti code, but improves performance by app. 10%
	if (hasCTF) {
		const float* __restrict__ CTF = getNthItem(CTFs, space->projectionIndex, fftSizeX, fftSizeY, false);
		const float* __restrict__ modulator = getNthItem(modulators, space->projectionIndex, fftSizeX, fftSizeY, false);

		// check which pixel in the vicinity should contribute
		for (int i = minY; i <= maxY; i++) {
			float ySqr = (imgPos.y - i) * (imgPos.y - i);
			float yzSqr = ySqr + zSqr;
			if (yzSqr > radiusSqr) continue;
			for (int j = minX; j <= maxX; j++) {
				float xD = imgPos.x - j;
				float distanceSqr = xD*xD + yzSqr;
				if (distanceSqr > radiusSqr) continue;

#if SHARED_IMG
				int index2D = (i - SHARED_AABB[0].y) * imgCacheDim + (j-SHARED_AABB[0].x); // position in img - offset of the AABB
#else
				int index2D = i * fftSizeX + j;
#endif

				float wCTF = CTF[index2D];
				float wModulator = modulator[index2D];
#if PRECOMPUTE_BLOB_VAL
				int aux = (int) ((distanceSqr * cIDeltaSqrt + 0.5f));
	#if SHARED_BLOB_TABLE
				float wBlob = BLOB_TABLE[aux];
	#else
				float wBlob = blobTableSqrt[aux];
	#endif
#else
				float wBlob = kaiserValue<blobOrder>(sqrtf(distanceSqr),cBlobRadius, cBlobAlpha);
#endif
				float weight = wBlob * wModulator * dataWeight;
				w += weight;
#if SHARED_IMG
				volReal += IMG[index2D].x * weight * wCTF;
				volImag += IMG[index2D].y * weight * wCTF;
#else
				volReal += img[2*index2D] * weight * wCTF;
				volImag += img[2*index2D + 1] * weight * wCTF;
#endif
			}
		}
	} else {
		// check which pixel in the vicinity should contribute
		for (int i = minY; i <= maxY; i++) {
			float ySqr = (imgPos.y - i) * (imgPos.y - i);
			float yzSqr = ySqr + zSqr;
			if (yzSqr > radiusSqr) continue;
			for (int j = minX; j <= maxX; j++) {
				float xD = imgPos.x - j;
				float distanceSqr = xD*xD + yzSqr;
				if (distanceSqr > radiusSqr) continue;

#if SHARED_IMG
				int index2D = (i - SHARED_AABB[0].y) * imgCacheDim + (j-SHARED_AABB[0].x); // position in img - offset of the AABB
#else
				int index2D = i * fftSizeX + j;
#endif

#if PRECOMPUTE_BLOB_VAL
				int aux = (int) ((distanceSqr * cIDeltaSqrt + 0.5f));
#if SHARED_BLOB_TABLE
				float wBlob = BLOB_TABLE[aux];
#else
				float wBlob = blobTableSqrt[aux];
#endif
#else
				float wBlob = kaiserValue<blobOrder>(sqrtf(distanceSqr),cBlobRadius, cBlobAlpha);
#endif
				float weight = wBlob * dataWeight;
				w += weight;
#if SHARED_IMG
				volReal += IMG[index2D].x * weight;
				volImag += IMG[index2D].y * weight;
#else
				volReal += img[2*index2D] * weight;
				volImag += img[2*index2D + 1] * weight;
#endif
			}
		}
	}
	// use atomic as two blocks can write to same voxel
	atomicAdd(&tempVolumeGPU[2*index3D], volReal);
	atomicAdd(&tempVolumeGPU[2*index3D + 1], volImag);
	atomicAdd(&tempWeightsGPU[index3D], w);
}

/**
  * Method will process one projection image and add result to temporal
  * spaces.
  */
template<bool useFast, bool hasCTF, int blobOrder>
__device__
void processProjection(
	float* tempVolumeGPU, float *tempWeightsGPU,
	const float* FFTs, const float* CTFs, const float* modulators,
	int fftSizeX, int fftSizeY,
	const RecFourierProjectionTraverseSpace* const tSpace,
	const float* devBlobTableSqrt,
	int imgCacheDim)
{
	// map thread to each (2D) voxel
	volatile int idx = blockIdx.x*blockDim.x + threadIdx.x;
	volatile int idy = blockIdx.y*blockDim.y + threadIdx.y;

	if (tSpace->XY == tSpace->dir) { // iterate XY plane
		if (idy >= tSpace->minY && idy <= tSpace->maxY) {
			if (idx >= tSpace->minX && idx <= tSpace->maxX) {
				if (useFast) {
					float hitZ;
					if (getZ(idx, idy, hitZ, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ)) {
						int z = (int)(hitZ + 0.5f); // rounding
						processVoxel<hasCTF>(tempVolumeGPU, tempWeightsGPU, FFTs, CTFs, modulators, fftSizeX, fftSizeY, idx, idy, z, tSpace);
					}
				} else {
					float z1, z2;
					bool hit1 = getZ(idx, idy, z1, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ); // lower plane
					bool hit2 = getZ(idx, idy, z2, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ); // upper plane
					if (hit1 || hit2) {
						z1 = clamp(z1, 0, cMaxVolumeIndexYZ);
						z2 = clamp(z2, 0, cMaxVolumeIndexYZ);
						int lower = floorf(fminf(z1, z2));
						int upper = ceilf(fmaxf(z1, z2));
						for (int z = lower; z <= upper; z++) {
							processVoxelBlob<hasCTF, blobOrder>(tempVolumeGPU, tempWeightsGPU, FFTs, CTFs, modulators, fftSizeX, fftSizeY, idx, idy, z, tSpace, devBlobTableSqrt, imgCacheDim);
						}
					}
				}
			}
		}
	} else if (tSpace->XZ == tSpace->dir) { // iterate XZ plane
		if (idy >= tSpace->minZ && idy <= tSpace->maxZ) { // map z -> y
			if (idx >= tSpace->minX && idx <= tSpace->maxX) {
				if (useFast) {
					float hitY;
					if (getY(idx, hitY, idy, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ)) {
						int y = (int)(hitY + 0.5f); // rounding
						processVoxel<hasCTF>(tempVolumeGPU, tempWeightsGPU, FFTs, CTFs, modulators, fftSizeX, fftSizeY, idx, y, idy, tSpace);
					}
				} else {
					float y1, y2;
					bool hit1 = getY(idx, y1, idy, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ); // lower plane
					bool hit2 = getY(idx, y2, idy, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ); // upper plane
					if (hit1 || hit2) {
						y1 = clamp(y1, 0, cMaxVolumeIndexYZ);
						y2 = clamp(y2, 0, cMaxVolumeIndexYZ);
						int lower = floorf(fminf(y1, y2));
						int upper = ceilf(fmaxf(y1, y2));
						for (int y = lower; y <= upper; y++) {
							processVoxelBlob<hasCTF, blobOrder>(tempVolumeGPU, tempWeightsGPU, FFTs, CTFs, modulators, fftSizeX, fftSizeY, idx, y, idy, tSpace, devBlobTableSqrt, imgCacheDim);
						}
					}
				}
			}
		}
	} else { // iterate YZ plane
		if (idy >= tSpace->minZ && idy <= tSpace->maxZ) { // map z -> y
			if (idx >= tSpace->minY && idx <= tSpace->maxY) { // map y > x
				if (useFast) {
					float hitX;
					if (getX(hitX, idx, idy, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ)) {
						int x = (int)(hitX + 0.5f); // rounding
						processVoxel<hasCTF>(tempVolumeGPU, tempWeightsGPU, FFTs, CTFs, modulators, fftSizeX, fftSizeY, x, idx, idy, tSpace);
					}
				} else {
					float x1, x2;
					bool hit1 = getX(x1, idx, idy, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ); // lower plane
					bool hit2 = getX(x2, idx, idy, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ); // upper plane
					if (hit1 || hit2) {
						x1 = clamp(x1, 0, cMaxVolumeIndexX);
						x2 = clamp(x2, 0, cMaxVolumeIndexX);
						int lower = floorf(fminf(x1, x2));
						int upper = ceilf(fmaxf(x1, x2));
						for (int x = lower; x <= upper; x++) {
							processVoxelBlob<hasCTF, blobOrder>(tempVolumeGPU, tempWeightsGPU, FFTs, CTFs, modulators, fftSizeX, fftSizeY, x, idx, idy, tSpace, devBlobTableSqrt, imgCacheDim);
						}
					}
				}
			}
		}
	}
}

/**
 * Method calculates an Axis Aligned Bounding Box in the image space.
 * AABB is guaranteed to be big enough that all threads in the block,
 * while processing the traverse space, will not read image data outside
 * of the AABB
 */
__device__
void calculateAABB(const RecFourierProjectionTraverseSpace* tSpace, Point3D<float>* dest) {
	Point3D<float> box[8];
	// calculate AABB for the whole working block
	if (tSpace->XY == tSpace->dir) { // iterate XY plane
		box[0].x = box[3].x = box[4].x = box[7].x = blockIdx.x*blockDim.x - cBlobRadius;
		box[1].x = box[2].x = box[5].x = box[6].x = (blockIdx.x+1)*blockDim.x + cBlobRadius - 1.f;

		box[2].y = box[3].y = box[6].y = box[7].y = (blockIdx.y+1)*blockDim.y + cBlobRadius - 1.f;
		box[0].y = box[1].y = box[4].y = box[5].y = blockIdx.y*blockDim.y- cBlobRadius;

		getZ(box[0].x, box[0].y, box[0].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getZ(box[4].x, box[4].y, box[4].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getZ(box[3].x, box[3].y, box[3].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getZ(box[7].x, box[7].y, box[7].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getZ(box[2].x, box[2].y, box[2].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getZ(box[6].x, box[6].y, box[6].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getZ(box[1].x, box[1].y, box[1].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getZ(box[5].x, box[5].y, box[5].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);
	} else if (tSpace->XZ == tSpace->dir) { // iterate XZ plane
		box[0].x = box[3].x = box[4].x = box[7].x = blockIdx.x*blockDim.x - cBlobRadius;
		box[1].x = box[2].x = box[5].x = box[6].x = (blockIdx.x+1)*blockDim.x + cBlobRadius - 1.f;

		box[2].z = box[3].z = box[6].z = box[7].z = (blockIdx.y+1)*blockDim.y + cBlobRadius - 1.f;
		box[0].z = box[1].z = box[4].z = box[5].z = blockIdx.y*blockDim.y- cBlobRadius;

		getY(box[0].x, box[0].y, box[0].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getY(box[4].x, box[4].y, box[4].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getY(box[3].x, box[3].y, box[3].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getY(box[7].x, box[7].y, box[7].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getY(box[2].x, box[2].y, box[2].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getY(box[6].x, box[6].y, box[6].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getY(box[1].x, box[1].y, box[1].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getY(box[5].x, box[5].y, box[5].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);
	} else { // iterate YZ plane
		box[0].y = box[3].y = box[4].y = box[7].y = blockIdx.x*blockDim.x - cBlobRadius;
		box[1].y = box[2].y = box[5].y = box[6].y = (blockIdx.x+1)*blockDim.x + cBlobRadius - 1.f;

		box[2].z = box[3].z = box[6].z = box[7].z = (blockIdx.y+1)*blockDim.y + cBlobRadius - 1.f;
		box[0].z = box[1].z = box[4].z = box[5].z = blockIdx.y*blockDim.y- cBlobRadius;

		getX(box[0].x, box[0].y, box[0].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getX(box[4].x, box[4].y, box[4].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getX(box[3].x, box[3].y, box[3].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getX(box[7].x, box[7].y, box[7].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getX(box[2].x, box[2].y, box[2].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getX(box[6].x, box[6].y, box[6].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);

		getX(box[1].x, box[1].y, box[1].z, tSpace, tSpace->bottomOriginX, tSpace->bottomOriginY, tSpace->bottomOriginZ);
		getX(box[5].x, box[5].y, box[5].z, tSpace, tSpace->topOriginX, tSpace->topOriginY, tSpace->topOriginZ);
	}
	// transform AABB to the image domain
	rotate(box, tSpace);
	// AABB is projected on image. Create new AABB that will encompass all vertices
	computeAABB(dest, box);
}

/**
 * Method returns true if AABB lies within the image boundaries
 */
__device__
bool isWithin(Point3D<float>* AABB, int imgXSize, int imgYSize) {
	return (AABB[0].x < imgXSize)
			&& (AABB[1].x >= 0)
			&& (AABB[0].y < imgYSize)
			&& (AABB[1].y >= 0);
}

/**
 * Method will load data from image at position tXindex, tYindex
 * and return them.
 * In case the data lies outside of the image boundaries, zeros (0,0)
 * are returned
 */
__device__
void getImgData(Point3D<float>* AABB,
		int tXindex, int tYindex,
		const float* FFTs, int fftSizeX, int fftSizeY,
		int imgIndex,
		float& vReal, float& vImag) {
	int imgXindex = tXindex + AABB[0].x;
	int imgYindex = tYindex + AABB[0].y;
	if ((imgXindex >=0)
			&& (imgXindex < fftSizeX)
			&& (imgYindex >=0)
			&& (imgYindex < fftSizeY))	{
		int index = imgYindex * fftSizeX + imgXindex; // copy data from image
		vReal = getNthItem(FFTs, imgIndex, fftSizeX, fftSizeY, true)[2*index];
		vImag = getNthItem(FFTs, imgIndex, fftSizeX, fftSizeY, true)[2*index + 1];

	} else {
		vReal = vImag = 0.f; // out of image bound, so return zero
	}
}

/**
 * Method will copy imgIndex(th) data from buffer
 * to given destination (shared memory).
 * Only data within AABB will be copied.
 * Destination is expected to be continuous array of sufficient
 * size (imgCacheDim^2)
 */
__device__
void copyImgToCache(float2* dest, Point3D<float>* AABB,
		const float* FFTs, int fftSizeX, int fftSizeY,
		int imgIndex,
		int imgCacheDim) {
	for (int y = threadIdx.y; y < imgCacheDim; y += blockDim.y) {
		for (int x = threadIdx.x; x < imgCacheDim; x += blockDim.x) {
			int memIndex = y * imgCacheDim + x;
			getImgData(AABB, x, y, FFTs, fftSizeX, fftSizeY, imgIndex, dest[memIndex].x, dest[memIndex].y);
		}
	}
}

/**
 * Method will use data stored in the buffer and update temporal
 * storages appropriately.
 */
template<bool useFast, bool hasCTF, int blobOrder>
__global__
void processBufferKernel(
		float* tempVolumeGPU, float *tempWeightsGPU,
		RecFourierProjectionTraverseSpace* spaces, int noOfSpaces,
		const float* FFTs, const float* CTFs, const float* modulators,
		int fftSizeX, int fftSizeY,
		float* devBlobTableSqrt,
		int imgCacheDim) {
#if SHARED_BLOB_TABLE
	if ( ! useFast) {
		// copy blob table to shared memory
		volatile int id = threadIdx.y*blockDim.x + threadIdx.x;
		volatile int blockSize = blockDim.x * blockDim.y;
		for (int i = id; i < BLOB_TABLE_SIZE_SQRT; i+= blockSize)
			BLOB_TABLE[i] = devBlobTableSqrt[i];
		__syncthreads();
	}
#endif

	for (int i = 0; i < noOfSpaces; i++) {
		RecFourierProjectionTraverseSpace* space = &spaces[i];

#if SHARED_IMG
		if ( ! useFast) {
			// make sure that all threads start at the same time
			// as they can come from previous iteration
			__syncthreads();
			if ((threadIdx.x == 0) && (threadIdx.y == 0)) {
				// first thread calculates which part of the image should be shared
				calculateAABB(space, SHARED_AABB);
			}
			__syncthreads();
			// check if the block will have to copy data from image
			if (isWithin(SHARED_AABB, fftSizeX, fftSizeY)) {
				// all threads copy image data to shared memory
				copyImgToCache(IMG, SHARED_AABB,
						FFTs, fftSizeX, fftSizeY,
						space->projectionIndex, imgCacheDim);
				__syncthreads();
			} else {
				continue; // whole block can exit, as it's not reading from image
			}
		}
#endif

		processProjection<useFast, hasCTF, blobOrder>(
			tempVolumeGPU, tempWeightsGPU,
			FFTs, CTFs, modulators, fftSizeX, fftSizeY,
			space,
			devBlobTableSqrt,
			imgCacheDim);
		__syncthreads(); // sync threads to avoid write after read problems
	}
}

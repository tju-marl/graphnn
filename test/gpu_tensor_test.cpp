#include "gtest/gtest.h"
#include "tensor/tensor_all.h"
#include <type_traits>

using namespace gnn;

#define sqr(x) ((x) * (x))

TEST(GPUTensorTest, ReshapeSize)
{
	GpuHandle::Init(0, 1);
	Tensor* t = new DTensor<GPU, float>();
	t->Reshape({2, 3, 4});

	auto& mat = t->Derived<GPU, DENSE, float>();

	ASSERT_EQ(2 * 3 * 4, mat.data->mem_size);
	GpuHandle::Destroy();
}

TEST(GPUTensorTest, RandUniform)
{
	GpuHandle::Init(0, 1);
	Tensor* t = new DTensor<GPU, double>();
	t->Reshape({101, 101, 101});	
	auto& tmat = t->Derived<GPU, DENSE, double>();
	tmat.SetRandU(-1.0, 3.0);
	auto* tmp = new DTensor<CPU, double>();
	tmp->CopyFrom(tmat);
	auto& mat = tmp->Derived<CPU, DENSE, double>();	
	double s = 0.0;
	for (size_t i = 0; i < mat.data->mem_size; ++i)
		s += mat.data->ptr[i];
	s /= mat.shape.Count();
	double err = fabs(s - 1.0);
	EXPECT_LE(err, 1e-3);
	GpuHandle::Destroy();
}

TEST(GPUTensorTest, RandNorm)
{
	GpuHandle::Init(0, 1);
	Tensor* t = new DTensor<GPU, double>();
	t->Reshape({100, 500, 100});	
	auto& tmat = t->Derived<GPU, DENSE, double>();

	tmat.SetRandN(5.0, 0.1);
	auto* tmp = new DTensor<CPU, double>();
	tmp->CopyFrom(tmat);
	auto& mat = tmp->Derived<CPU, DENSE, double>();	

	double s = 0.0;
	for (size_t i = 0; i < mat.data->mem_size; ++i)
		s += mat.data->ptr[i];
	s /= mat.shape.Count();
	double err = fabs(s - 5.0);
	EXPECT_LE(err, 1e-4);

	double ss = 0.0;
	for (size_t i = 0; i < mat.data->mem_size; ++i)
		ss += sqr(mat.data->ptr[i] - s);
	ss = sqrt(ss / mat.shape.Count());
	err = fabs(ss - 0.1);
	EXPECT_LE(err, 1e-4);
	GpuHandle::Destroy();
}

TEST(GPUTensorTest, Fill)
{
	GpuHandle::Init(0, 1);
	DTensor<GPU, float> mat;
	mat.Reshape({100, 100, 100});
	mat.Fill(2.0);

	float ans = mat.ASum();

	ASSERT_EQ(2 * 100 * 100 * 100, ans);
	GpuHandle::Destroy();
}

TEST(GPUTensorTest, ArgMax)
{
	GpuHandle::Init(0, 1);

	DTensor<CPU, float> d_cpu;
	DTensor<CPU, int> idx_cpu, buf;
	d_cpu.Reshape({10, 1023});
	d_cpu.SetRandN(0.0, 1.0);
	d_cpu.ArgMax(idx_cpu);

	DTensor<GPU, float> d_gpu;
	DTensor<GPU, int> idx_gpu;
	d_gpu.CopyFrom(d_cpu);
	d_gpu.ArgMax(idx_gpu);
	buf.CopyFrom(idx_gpu);

	for (size_t i = 0; i < idx_gpu.shape.Count(); ++i)
	{
		ASSERT_EQ(idx_cpu.data->ptr[i], buf.data->ptr[i]);
	}
	GpuHandle::Destroy();
}

TEST(GPUTensorTest, GeMM)
{
	GpuHandle::Init(0, 1);

	DTensor<CPU, float> x, y, z, zz;
	x.Reshape({10, 20});
	y.Reshape({30, 20});

	x.SetRandN(0.0, 1.0);
	y.SetRandN(0.0, 1.0);
	z.MM(x, y, Trans::N, Trans::T, 1.0, 0.0);

	DTensor<GPU, float> gx, gy, gz;
	gx.CopyFrom(x);
	gy.CopyFrom(y);
	gz.MM(gx, gy, Trans::N, Trans::T, 1.0, 0.0);

	zz.CopyFrom(gz);
	auto a1 = z.ASum(), a2 = gz.ASum();
	EXPECT_LE(fabs(a1 - a2), 1e-4);

	GpuHandle::Destroy();
}

TEST(GPUTensorTest, Softmax)
{
	GpuHandle::Init(0, 1);

	DTensor<CPU, float> x, y;
	DTensor<GPU, float> gx;
	x.Reshape({20, 200});
	x.SetRandN(0.0, 1.0);
	gx.CopyFrom(x);

	x.Softmax();
	gx.Softmax();
	y.CopyFrom(gx);

	x.Axpy(-1.0, y);

	EXPECT_LE(x.ASum(), 1e-4);

	GpuHandle::Destroy();
}

TEST(GPUTensorTest, Mean)
{
	GpuHandle::Init(0, 1);

	DTensor<CPU, float> x, dst_x;
	DTensor<GPU, float> gx, dst_gx;
	x.Reshape({20, 200});
	x.SetRandU(1, 2);
	gx.CopyFrom(x);

	dst_x.Mean(x);
	dst_gx.Mean(gx);

	EXPECT_LE(fabs(dst_x.AsScalar() - dst_gx.AsScalar()), 1e-5);

	GpuHandle::Destroy();
}

TEST(GPUTensorTest, ElewiseMul)
{
	GpuHandle::Init(0, 1);

	DTensor<CPU, float> x, y, tmp;
	DTensor<GPU, float> gx, gy;
	x.Reshape({20, 200});
	x.SetRandU(1, 2);
	y.Reshape({20, 200});
	y.SetRandN(0, 2);		

	gx.CopyFrom(x);
	gy.CopyFrom(y);

	x.ElewiseMul(y);
	gx.ElewiseMul(gy);

	tmp.CopyFrom(gx);

	x.Axpy(-1.0, tmp);

	EXPECT_LE(x.ASum(), 1e-4);

	GpuHandle::Destroy();
}

TEST(GPUTensorTest, BroadcastMulCol)
{
	GpuHandle::Init(0, 1);
	DTensor<CPU, float> x, y;
	x.Reshape({5, 3}); 
	y.Reshape({5, 1});
	x.Fill(1.0);
	for (int i = 0; i < 5; ++i)
	{
		x.data->ptr[i * 3 + 1] = 2.0;
		x.data->ptr[i * 3 + 2] = 3.0;
	}
	for (int i = 0; i < 5; ++i)
		y.data->ptr[i] = i + 1;

	DTensor<GPU, float> gx, gy;
	gx.CopyFrom(x);
	gy.CopyFrom(y);

	gx.ElewiseMul(gy);
	x.Fill(0);
	x.CopyFrom(gx);
	for (int i = 0; i < (int)x.rows(); ++i)
		for (int j = 0; j < (int)x.cols(); ++j)
			ASSERT_EQ(x.data->ptr[i * x.cols() + j], (i + 1) * (j + 1));
	GpuHandle::Destroy();		
}

TEST(GPUTensorTest, BroadcastMulRow)
{
	GpuHandle::Init(0, 1);
	DTensor<CPU, float> x, y;
	x.Reshape({30, 50}); 
	y.Reshape({1, 50});

	x.SetRandN(0, 1);
	y.SetRandN(0, 1);

	DTensor<GPU, float> gx, gy;
	gx.CopyFrom(x);
	gy.CopyFrom(y);

	gx.ElewiseMul(gy);	
	x.ElewiseMul(y);

	DTensor<CPU, float> tx;
	tx.CopyFrom(gx);
	x.Axpy(-1, tx);

	EXPECT_LE(x.ASum(), 1e-4);
	GpuHandle::Destroy();
}

TEST(GPUTensorTest, InvSqrSqrtNorm2)
{
	GpuHandle::Init(0, 1);

	DTensor<CPU, float> x, tmp;
	x.Reshape({10, 10}); 
	x.SetRandU(1, 3);
	DTensor<GPU, float> gx;
	gx.CopyFrom(x);

	x.Square();
	gx.Square();
	tmp.CopyFrom(gx);
	tmp.Axpy(-1, x);
	EXPECT_LE(tmp.ASum(), 1e-4);

	x.Sqrt();
	gx.Sqrt();
	tmp.CopyFrom(gx);
	tmp.Axpy(-1, x);
	EXPECT_LE(tmp.ASum(), 1e-4);

	x.Inv();
	gx.Inv();
	tmp.CopyFrom(gx);
	tmp.Axpy(-1, x);
	EXPECT_LE(tmp.ASum(), 1e-4);
	
	EXPECT_LE(fabs(x.Norm2() - gx.Norm2()), 1e-4);
	GpuHandle::Destroy();
}

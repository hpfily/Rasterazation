//#include "stdafx.h"
#pragma comment ( lib, "D3D11.lib")
#include "windows.h"
#include <iostream>
#include <d3d11.h>
#include <d3dx11.h>
#include <math.h>

#define  PI 3.1415926
#define  hudu(x)  (x*PI/180) 

#define COLOR(r,g,b) (r<<12+b<<8+g<<4+0xFF)

using namespace std;
typedef unsigned char U8;
typedef UINT U32;

const int WIDTH = 800;
const int HEIGHT = 600;
const int _COLOR = 0xFF00FFFF;   //R B G A
const int CenX = WIDTH / 2;
const int CenY = HEIGHT / 2;

HINSTANCE               g_hInst = NULL;
HWND                    g_hWnd = NULL;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice = NULL;
ID3D11DeviceContext*    g_pImmediateContext = NULL;
IDXGISwapChain*         g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11Texture2D*                g_pTempTexture = NULL;
UINT*                                   g_pFB[HEIGHT];

typedef struct DrawVertex
{
	struct
	{
		int x;
		int y;
		int z;
	};
#if 0
	union
	{
		struct  //R B G A
		{
			U8      A;
			U8      G;
			U8      B;
			U8      R;
		};
		U32 color;
	};
#endif
	
#if 1
	void operator= (DrawVertex right)
	{
		memcpy(this, &right, sizeof(DrawVertex));
	}
#endif
}DrawVertex;

typedef struct vec4
{
	union
	{
		float v[4];
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
	};

	vec4(float x,float y,float z,float w):x(x),y(y),z(z),w(w){};
	vec4():x(0),y(0),z(0),w(0){};
	void operator=(vec4 right)
	{
		memcpy(this,&right,sizeof(vec4));
	}
	
}vec4;

typedef struct matrix4
{
	float m[4][4];
	matrix4()
	{
		memset(&m[0][0],0,4*4*sizeof(float));
		for(int i=0;i<4;i++)
		{
			m[i][i]=1.0f;
		}
	}
	matrix4(float src[4][4])
	{
		memcpy(this->m,src,sizeof(this->m));
	}
	vec4 operator*(vec4 &right)
	{
		vec4 res;
		for (int aRow=0;aRow<4;aRow++)
		{
			for (int aCol=0;aCol<4;aCol++)
			{
				res.v[aRow]+=m[aRow][aCol]*right.v[aCol];
			}
		}

		return res;
	}
	matrix4 operator*(matrix4 &right)
	{
		matrix4 res;
		memset(res.m,0,sizeof(res.m));
		
		for (int aRow=0;aRow<4;aRow++)
		{
			for (int bCol=0;bCol<4;bCol++)
			{
				for (int aCol=0;aCol<4;aCol++)
				{
					res.m[aRow][bCol]+=m[aRow][aCol]*right.m[aCol][bCol];
				}
			}
		}
		return res;

	}

	void operator=(matrix4 right)
	{
		memcpy(this,&right,sizeof(matrix4));
	}

}matrix4;

typedef struct Camera :public vec4
{
	Camera(float x,float y,float z,float w):vec4(x,y,z,w){};
	matrix4 getViewTrasMat(float theta)
	{
		matrix4 View;
		for (int i=0;i<3;i++)
		{
			View.m[i][3]=-this->v[i];
		}
		float r[4][4]=
		{
			{0,cos(theta),0,-sin(theta)},
			{0,1,0,0},
			{sin(theta),0,cos(theta),0},
			{0,0,0,1}
		};
		matrix4 rot(r);
		View=View*rot;
		
		return View;
	}
}Camera;

#ifdef _DEBUG 
void MyPrint(char* format, ...)
{
	va_list args = NULL;
	va_start(args, format);
	char buf[256];
	_vsnprintf_s(buf, 256, format, args);
	OutputDebugStringA(buf);
	va_end(args);
}
#else 
void MyPrint(_TCHAR* format, ...) {}
#endif 


float interp(float factor, int x0, int x1) { return (x1 - x0)*factor + x0 + 0.5f; }

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
int InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_APPLICATION);
	wcex.lpszClassName = L"FireEngine";
	if (!RegisterClassEx(&wcex))
	{
		return -1;
	}
	g_hInst = hInstance;
	RECT rc = { 0,0,WIDTH,HEIGHT };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"FireEngine", L"Test1", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);
	if (!g_hWnd)
	{
		return -1;
	}
	ShowWindow(g_hWnd, nCmdShow);
	return 0;

}

HRESULT InitDevice()
{
	HRESULT hr = S_OK;
	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;
	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_DRIVER_TYPE DriverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = sizeof(DriverTypes) / sizeof(D3D10_DRIVER_TYPE);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for (UINT driverTypeIndex = 0; driverTypeIndex<numDriverTypes; driverTypeIndex++)
	{
		g_driverType = DriverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		if (SUCCEEDED(hr))
		{
			break;
		}
	}
	if (FAILED(hr))
	{
		return hr;
	}

	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
	{
		return hr;
	}
	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

	D3D11_VIEWPORT vp;
	vp.Width = (float)width;
	vp.Height = (float)height;
	vp.MaxDepth = 1.0f;
	vp.MinDepth = 0.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	g_pImmediateContext->RSSetViewports(1, &vp);


	ID3D11Texture2D *pFrameBuffer = NULL;
	D3D11_TEXTURE2D_DESC Desc = { 0 };
	Desc.MipLevels = Desc.ArraySize = 1;
	Desc.Width = WIDTH;
	Desc.Height = HEIGHT;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = g_pd3dDevice->CreateTexture2D(&Desc, NULL, &pFrameBuffer);
	if (FAILED(hr))
	{
		return hr;
	}
	g_pTempTexture = pFrameBuffer;

	return S_OK;
}
void DrawPixel(int x, int y, UINT color)
{
	int U = CenX + x;
	int V = CenY - y;
	g_pFB[V][U] = color;
}
void DrawLine(int x0, int y0, int x1, int y1, UINT color)
{
	int dx = x1 - x0;
	int dy = y1 - y0;
	int error = 0;
	int xstep, ystep;
	int x = x0, y = y0;
	xstep = dx>0 ? 1 : -1;
	ystep = dy>0 ? 1 : -1;
	dx = dx>0 ? dx : -dx;
	dy = dy>0 ? dy : -dy;
	if (dx>dy)
	{
		error = dx / 2;

		for (int i = 0; i <= dx; i++)
		{
			DrawPixel(x, y, _COLOR);
			error += dy;
			if (error >= dx)
			{
				error -= dx;
				y += ystep;
			}
			x += xstep;
		}
	}
	else
	{
		error = dx / 2;

		for (int i = 0; i <= dy; i++)
		{
			DrawPixel(x, y, _COLOR);
			error += dx;
			if (error >= dy)
			{
				error -= dy;
				x += xstep;
			}
			y += ystep;
		}
	}
}

void DrawUPTriangle(DrawVertex* v0, DrawVertex* v1, DrawVertex* v2)
{
	int x0, x1, x2, y0, y1, y2;
	x0 = v0->x, x1 = v1->x, x2 = v2->x;
	y0 = v0->y, y1 = v1->y, y2 = v2->y;

	int yL, yR;
	float factorL, factorR, xL, xR;
	for (yL = y1; yL <= y0; yL++)
	{
		yR = yL;
		factorL = ((float)yL - y1) / (y0 - y1);
		factorR = ((float)yR - y2) / (y0 - y2);
		xL = interp(factorL, x1, x0);
		xR = interp(factorR, x2, x0);
		DrawLine(xL, yL, xR, yR, _COLOR);
	}
}
void DrawDownTriangle(DrawVertex* v0, DrawVertex* v1, DrawVertex* v2)
{
	int x0, x1, x2, y0, y1, y2;
	x0 = v0->x, x1 = v1->x, x2 = v2->x;
	y0 = v0->y, y1 = v1->y, y2 = v2->y;

	int yL, yR;
	float factorL, factorR, xL, xR;
	for (yL = y0; yL >= y2; yL--)
	{
		yR = yL;
		factorL = ((float)yL - y0) / (y2 - y0);
		factorR = ((float)yR - y1) / (y2 - y1);
		xL = interp(factorL, x0, x2);
		xR = interp(factorR, x1, x2);
		DrawLine(xL, yL, xR, yR, _COLOR);
	}
}
void SortDrawVertex(DrawVertex *V[])
{
	//v0 -> v1 -> v2  high-> low 
	for (int i = 0; i<3; i++)
	{
		for (int j = i; j<3; j++)
		{
			if (V[i]->y < V[j]->y)
			{
				swap(V[i], V[j]);
			}
			// MyPrint("v0 =%d, v1=%d, v2=%d  j=%d \n", V[0]->y, V[1]->y, V[2]->y, j);
		}
	}

	return;
}
void DrawTriangle(DrawVertex V0, DrawVertex V1, DrawVertex V2)
{

	DrawVertex* V[3] = { &V0,&V1,&V2 };   //V0 y max   V2 y min
	SortDrawVertex(V);
	if (V[1]->y==V[2]->y)
	{
		DrawUPTriangle(V[0], V[1], V[2]);
	}
	else if (V[0]->y == V[1]->y)
	{
		DrawDownTriangle(V[0], V[1], V[2]);
	}
	else
	{
		DrawVertex vadd;
		vadd.y = V[1]->y;
		float factor = ((float)vadd.y - V[2]->y) / (V[0]->y - V[2]->y);
		vadd.x = interp(factor, V[2]->x, V[0]->x);
		DrawUPTriangle(V[0], V[1], &vadd);
		DrawDownTriangle(V[1], &vadd, V[2]);
	}
}
void my_circle(void* fb)
{
	int w = 800, h = 600;
	int centerX = w / 2, centerY = h / 2;
	int r = 200;
	int *framebuffer = (int*)fb;
	int u = 0;
	int v = 0;
	for (u = 0; u<w; u++)
	{
		for (v = 0; v<h; v++)
		{
			int x = u - centerX, y = v - centerY;
			if (x*x + y*y <= r*r)
			{
				//                              *(framebuffer+(w*v)+u)=0xFFFF00FF;
				g_pFB[v][u] = 0xFFFF00FF;
			}
		}
	}
}
void Render()
{
#if 0
	//Map_Write_discard : clear will discard.
	float ClearColor[4] = { 0.0f,0.5f,0.5f,1.0f };
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
	g_pSwapChain->Present(0, 0);
#endif

	ID3D11Texture2D *pBackBuffer = NULL;
	HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
	{
		return;
	}
	D3D11_MAPPED_SUBRESOURCE MapBuffer;
	hr = g_pImmediateContext->Map(g_pTempTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &MapBuffer);
	if (FAILED(hr))
	{
		return;
	}
	memset(MapBuffer.pData, 0, WIDTH*HEIGHT * sizeof(UINT));
	for (UINT i = 0; i<HEIGHT; i++)
	{
		g_pFB[i] = &((UINT*)MapBuffer.pData)[i*WIDTH];

	}
#if 0
	DrawVertex p0; p0.x=-100,p0.y=100;
	DrawVertex p1; p1.x=100,p1.y=100;
	DrawVertex p2; p2.x=-100,p2.y=-100;
	DrawVertex p3; p3.x=100, p3.y=-100;
	DrawTriangle(p0, p1, p2);
	DrawTriangle(p1, p2, p3);
#endif 
	vec4 v0(0.5f,0.0f,0.0f,1);
	vec4 v1(-0.5f,0.0f,0.0f,1);
	vec4 v2(0.0f,0.5f,1.0f,1);

	Camera cam(0.0f,0.0f,-2.0f,1);
	static int a=30;
	matrix4 matView= cam.getViewTrasMat(hudu(a));
	a+=30;
	vec4 t0=matView*v0;
	vec4 t1=matView*v1;
	vec4 t2=matView*v2;
	
	t0.x/=t0.z,t0.y/=t0.z;
	t1.x/=t1.z,t1.y/=t1.z;
	t2.x/=t2.z,t2.y/=t2.z;

	DrawVertex p0; p0.x=t0.x*WIDTH,p0.y=t0.y*WIDTH;
	DrawVertex p1; p1.x=t1.x*WIDTH,p1.y=t1.y*WIDTH;
	DrawVertex p2; p2.x=t2.x*WIDTH,p2.y=t2.y*WIDTH;
	DrawTriangle(p0, p1, p2);
	
	g_pImmediateContext->Unmap(g_pTempTexture, 0);
	g_pImmediateContext->CopyResource(pBackBuffer, g_pTempTexture);
	g_pSwapChain->Present(0, 0);
	int ij = 0;
}
void CleanupDevice()
{
	g_pImmediateContext->Flush();
	if (g_pTempTexture) g_pTempTexture->Release();
	if (g_pImmediateContext) g_pImmediateContext->ClearState();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
#if defined(DEBUG) || defined(_DEBUG)
	ID3D11Debug *d3dDebug;
	HRESULT hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&d3dDebug));
	if (SUCCEEDED(hr))
	{
		hr = d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
	}
	if (d3dDebug != NULL)                   d3dDebug->Release();
#endif
	if (g_pd3dDevice) g_pd3dDevice->Release();

}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{

	if (InitWindow(hInstance, nCmdShow))
	{
		return ERROR;
	}
	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return ERROR;
	}
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}
	CleanupDevice();
	return (int)msg.wParam;
}
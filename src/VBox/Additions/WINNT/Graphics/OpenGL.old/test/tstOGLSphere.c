#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <windows.h>
#include <mmsystem.h>

#include <gl/gl.h>

#define PI_OVER_180    0.01745329F
#define DEG2RAD( a )   ( (a) * PI_OVER_180 )

#define ELEMENTS_PER_VERTEX 3
#define ELEMENTS_PER_NORMAL 3
#define BYTES_PER_CACHELINE 32

/* struct used to manage color ramps */
struct colorIndexState {
    GLfloat amb[3];	/* ambient color / bottom of ramp */
    GLfloat diff[3];	/* diffuse color / middle of ramp */
    GLfloat spec[3];	/* specular color / top of ramp */
    GLfloat ratio;	/* ratio of diffuse to specular in ramp */
    GLint indexes[3];	/* where ramp was placed in palette */
};

#define NUM_COLORS (sizeof(s_colors) / sizeof(s_colors[0]))
struct colorIndexState s_colors[] = {
    {
        { 0.0F, 0.0F, 0.0F },
        { 1.0F, 0.0F, 0.0F },
        { 1.0F, 1.0F, 1.0F },
        0.75F, { 0, 0, 0 },
    },
    {
        { 0.0F, 0.05F, 0.05F },
        { 0.9F, 0.0F, 1.0F },
        { 1.0F, 1.0F, 1.0F },
        1.0F, { 0, 0, 0 },
    },
    {
        { 0.0F, 0.0F, 0.0F },
        { 1.0F, 0.9F, 0.1F },
        { 1.0F, 1.0F, 1.0F },
        0.75F, { 0, 0, 0 },
    },
    {
        { 0.0F, 0.0F, 0.0F },
        { 0.1F, 1.0F, 0.9F },
        { 1.0F, 1.0F, 1.0F },
        0.75F, { 0, 0, 0 },
    },
};

static void (APIENTRY *LockArraysSGI)(GLint first, GLsizei count);
static void (APIENTRY *UnlockArraysSGI)(void);
static void (APIENTRY *CullParameterfvSGI)( GLenum pname, GLfloat *params );

static GLint s_lit_tex_indexes[3];

static int s_num_rows = 16;
static int s_num_cols = 16;

static int s_winwidth  = 320;
static int s_winheight = 240;

#define MS_TO_RENDER       5000

#define DRAW_VERTEX3FV          0
#define DRAW_DRAW_ELEMENTS      1
#define DRAW_DRAW_ARRAYS        2
#define DRAW_ARRAY_ELEMENT      3

static const char *s_class_name  = "GL Sphere";
static const char *s_window_name = "GL Sphere";

static BOOL    s_rgba        = TRUE;
static BOOL    s_lighting    = TRUE;
static BOOL    s_benchmark   = FALSE;
static BOOL    s_remote      = FALSE;
static BOOL    s_lock_arrays = TRUE;
static BOOL    s_vcull       = TRUE;

static HPALETTE s_hPalette = NULL;
static HWND     s_hWnd  = NULL;
static HGLRC    s_hglrc = NULL;
static HDC      s_hDC   = NULL;
static int      s_bpp   = 8;
static int      s_draw_method = DRAW_VERTEX3FV;

static unsigned long s_vertices_processed = 0L;
static unsigned long s_triangles_processed = 0L;
static float         s_elapsed_time;
static unsigned long s_start, s_stop;

/*
** this maintains the data for drawing stuff via tristrips
*/
static float **s_sphere_points;
static float **s_sphere_normals;

/*
** this maintains the data for drawing stuff via vertex arrays and array elements
*/
static float  *s_sphere_point_array;
static float  *s_sphere_normal_array;

/*
** this stores the data for drawing stuff using interleaved arrays with
** draw elements
*/
static float           *s_sphere_interleaved_array;
static unsigned int   **s_sphere_elements;

/*
** this maintains the data for drawing stuff via vertex arrays and drawarrays
*/
static float  **s_sphere_point_draw_array;
static float  **s_sphere_normal_draw_array;

static BOOL    SphereCreateGeometry( int rows, int cols, float scale );
static BOOL    InitSphere( HINSTANCE hInstance, LPSTR lpszCmdLine );
static void    ShutdownSphere( void );
static void    SphereSetDefaults( void );
static void    SphereNormalizeVector( float v[3] );
static void    SphereHelp( void );
static void    SphereSetupPalette( HDC hDC );

static void *AllocAlignedX( int num_bytes, int byte_alignment );

static void SphereRedraw( void );

static BOOL SphereSetupPixelFormat( HDC hDC );

static LRESULT APIENTRY WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                      LPSTR lpszCmdLine,
                      int nCmdShow )
{
   MSG msg;

   if ( InitSphere( hInstance, lpszCmdLine ) )
   {
      ShowWindow( s_hWnd, nCmdShow );
      UpdateWindow( s_hWnd );

      while ( 1 )
      {
         if ( s_benchmark )
         {
            int i;
            int vertices_to_render = 1000000;
            int frames_to_render = vertices_to_render / ( s_num_rows * ( s_num_cols * 2 + 2 ) );

            if ( frames_to_render < 1 ) frames_to_render = 1;

            s_start = timeGetTime();

            for ( i = 0; i < frames_to_render; i++ )
            {
               SphereRedraw();
            }
            glFinish();
            s_stop = timeGetTime();
            s_elapsed_time = ( s_stop - s_start ) / 1000.0F;
            ShutdownSphere();
            return 0;
         }
         else
         {
            while ( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) == FALSE )
            {
               SphereRedraw();
            }
            if ( GetMessage( &msg, NULL, 0, 0 ) != TRUE )
            {
               ShutdownSphere();
               return msg.wParam;
            }
            TranslateMessage( &msg );
            DispatchMessage( &msg );
         }
      }
   }

   return 0;
}

static LRESULT APIENTRY WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch ( msg )
   {
   case WM_CREATE:
      s_hDC = GetDC( hWnd );

      if ( !SphereSetupPixelFormat( s_hDC ) )
         exit( 1 );
      SphereSetupPalette( s_hDC );
      s_hglrc = wglCreateContext( s_hDC );
      wglMakeCurrent( s_hDC, s_hglrc );
      /*
      ** see what kind of extensions are supported
      */
      if ( strstr( glGetString( GL_EXTENSIONS ), "GL_SGI_compiled_vertex_array " ) != 0 )
      {
          LockArraysSGI   = ( void (APIENTRY *)(GLint, GLint) ) wglGetProcAddress( "glLockArraysSGI" );
          UnlockArraysSGI = ( void (APIENTRY *)( void ) )       wglGetProcAddress( "glUnlockArraysSGI" );
      }
#ifdef GL_SGI_cull_vertex
      if ( s_vcull )
      {
          if ( strstr( glGetString( GL_EXTENSIONS ), "GL_SGI_cull_vertex" ) )
          {
              if ( ( CullParameterfvSGI = ( void (APIENTRY *)(GLenum, GLfloat*) ) wglGetProcAddress( "glCullParameterfvSGI" ) ) == 0 )
              {
                  MessageBox( 0, "-vcull specified but wglGetProcAddress failed", "Sphere Warning", MB_OK | MB_ICONINFORMATION );
                  s_vcull = FALSE;
              }
          }
          else
          {
              s_vcull = FALSE;
              MessageBox( 0, "-vcull specified but no vcull extension exists", "Sphere Warning", MB_OK | MB_ICONINFORMATION );
          }
      }
#endif

      SphereSetDefaults();

      /*
      ** set vertex and normal array pointers
      */
      if ( s_draw_method == DRAW_ARRAY_ELEMENT )
      {
	  glEnableClientState( GL_VERTEX_ARRAY );
	  glEnableClientState( GL_NORMAL_ARRAY );
	  glVertexPointer( ELEMENTS_PER_VERTEX, GL_FLOAT, 0, ( const GLvoid * ) s_sphere_point_array );
	  glNormalPointer( GL_FLOAT, ELEMENTS_PER_NORMAL * sizeof( float ) , ( const GLfloat * ) s_sphere_normal_array );
      }
      else if ( s_draw_method == DRAW_DRAW_ARRAYS )
      {
	  glEnableClientState( GL_VERTEX_ARRAY );
	  glEnableClientState( GL_NORMAL_ARRAY );
      }
      else if ( s_draw_method == DRAW_DRAW_ELEMENTS )
      {
      }

      {
         char  buffer[1000], buf2[1000];
         char *dummy;
         
         SearchPath( NULL, "opengl32.dll", NULL, 1000, buffer, &dummy );
         sprintf( buf2, "GL Sphere (%s)", buffer );
         SetWindowText( hWnd, buf2 );
      }
      return 0;
   case WM_DESTROY:
      PostQuitMessage( 0 );
      return 0;
   case WM_CHAR:	
      switch ( wParam )
      {
      case 'd':
          s_draw_method = DRAW_DRAW_ELEMENTS;
          glDisableClientState( GL_VERTEX_ARRAY );
          glDisableClientState( GL_NORMAL_ARRAY );
          break;
      case 'a':
          s_draw_method = DRAW_DRAW_ARRAYS;
          glEnableClientState( GL_VERTEX_ARRAY );
          glEnableClientState( GL_NORMAL_ARRAY );
          break;
      case 'l':
          s_lock_arrays = !s_lock_arrays;
          break;
      case 27:
         PostQuitMessage( 0 );
         return 0;
      }
      break;
   case WM_SIZE:
       if ( s_hglrc )
       {
           int winWidth  = ( int ) LOWORD( lParam );
           int winHeight = ( int ) HIWORD( lParam );

           glViewport( 0, 0, winWidth, winHeight );
       }
       return 0;
   case WM_PALETTECHANGED:
       if (s_hPalette != NULL && (HWND) wParam != hWnd) 
       {
           UnrealizeObject( s_hPalette );
           SelectPalette( s_hDC, s_hPalette, FALSE );
           RealizePalette( s_hDC );
           return 0;
       }
       break;
   case WM_QUERYNEWPALETTE:
       if ( s_hPalette != NULL ) 
       {
           UnrealizeObject( s_hPalette );
           SelectPalette( s_hDC, s_hPalette, FALSE );
           RealizePalette( s_hDC );
           return TRUE;
       }
       break;
   case WM_COMMAND:
      break;
   }

   return DefWindowProc( hWnd, msg, wParam, lParam);
}

static BOOL InitSphere( HINSTANCE hInstance, LPSTR lpszCmdLine )
{
   WNDCLASS wc;
   
   wc.style       = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
   wc.lpfnWndProc = WndProc;
   wc.cbClsExtra  = 0;
   wc.cbWndExtra  = 0;
   wc.hInstance   = hInstance;
   wc.hIcon       = LoadIcon(NULL, IDI_APPLICATION);
   wc.hCursor     = LoadCursor(NULL, IDC_ARROW);
   wc.hbrBackground = GetStockObject(WHITE_BRUSH);
   wc.lpszMenuName = NULL;
   wc.lpszClassName = s_class_name;
   
   RegisterClass( &wc );
   
   {
      HDC hdc = GetDC( 0 );
      
      s_bpp = GetDeviceCaps( hdc, BITSPIXEL );

#if NOTYET
      if ( ( s_bpp <= 8 ) && !strstr( lpszCmdLine, "-332" ) )
         s_rgba = 0;
      else 
#endif

      s_rgba = 1;
      
      ReleaseDC( 0, hdc );
   }

   /*
   ** check to see how to draw the sphere
   */
   if ( strstr( lpszCmdLine, "-arrayelement" ) )
      s_draw_method = DRAW_ARRAY_ELEMENT;
   else if ( strstr( lpszCmdLine, "-drawarrays" ) )
      s_draw_method = DRAW_DRAW_ARRAYS;
   else if ( strstr( lpszCmdLine, "-drawelements" ) )
      s_draw_method = DRAW_DRAW_ELEMENTS;
   else
      s_draw_method = DRAW_VERTEX3FV;

   if ( strstr( lpszCmdLine, "-benchmark" ) )
      s_benchmark = TRUE;
   if ( strstr( lpszCmdLine, "-remote" ) )
      s_remote = TRUE;

   if ( strstr( lpszCmdLine, "-rows:" ) )
   {
      s_num_rows = atoi( strchr( strstr( lpszCmdLine, "-rows:" ), ':' ) + 1 );
   }
   if ( strstr( lpszCmdLine, "-cols:" ) )
   {
      s_num_cols = atoi( strchr( strstr( lpszCmdLine, "-cols:" ), ':' ) + 1 );
   }

   if ( strstr( lpszCmdLine, "-nolockarrays" ) )
      s_lock_arrays = FALSE;

   if ( strstr( lpszCmdLine, "-novcull" ) )
       s_vcull = FALSE;

   if ( strstr( lpszCmdLine, "-?" ) || strstr( lpszCmdLine, "/?" ) )
   {
      SphereHelp();
      return FALSE;
   }

   if ( strstr( lpszCmdLine, "-width:" ) )
       s_winwidth  = atoi( strchr( strstr( lpszCmdLine, "-width:" ), ':' ) + 1 );
   if ( strstr( lpszCmdLine, "-height:" ) )
       s_winheight = atoi( strchr( strstr( lpszCmdLine, "-height:" ), ':' ) + 1 );

   /*
   ** create geometry
   */
   if ( !SphereCreateGeometry( s_num_rows, s_num_cols, 1.0F ) )
      return FALSE;

   /*
   ** create a window of the previously defined class
   */
   s_hWnd = CreateWindow( s_class_name,
                          s_window_name, 
                          WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          10, 10, 
                          s_winwidth, s_winheight,
                          NULL,
                          NULL,
                          hInstance,
                          NULL );

   if ( s_hWnd != NULL )
      return TRUE;
   return FALSE;
}

void ShutdownSphere( void )
{
   if ( s_benchmark && !s_remote )
   {
      char buffer[1000]="";
      char temp[100];

      sprintf( temp, "Vendor: %s\n\n", glGetString( GL_VENDOR ) );
      strcat( buffer, temp );

      sprintf( temp, "%.2f vertices/second\n", s_vertices_processed / s_elapsed_time );
      strcat( buffer, temp );

      sprintf( temp, "%.2f triangles/second\n", s_triangles_processed / s_elapsed_time );
      strcat( buffer, temp );

      MessageBox( 0, buffer, "Performance", MB_OK | MB_ICONINFORMATION );
   }

   if ( s_hglrc )
   {
      wglMakeCurrent( NULL, NULL );
      wglDeleteContext( s_hglrc );
   }
   ReleaseDC( s_hWnd, s_hDC );

}

static BOOL SphereSetupPixelFormat( HDC hDC )
{
   unsigned char pixelType = s_rgba ? PFD_TYPE_RGBA : PFD_TYPE_COLORINDEX;
   PIXELFORMATDESCRIPTOR pfd = 
   {
      sizeof( PIXELFORMATDESCRIPTOR ),
      1,
      PFD_DRAW_TO_WINDOW |	PFD_SUPPORT_OPENGL |	PFD_DOUBLEBUFFER,
      pixelType,
      s_bpp,
      0, 0, 0, 0, 0, 0,
      0,
      0,
      0,
      0, 0, 0, 0,
      16,                 // 16-bit depth buffer
      0,                  // no stencil buffer
      0,                  // no aux buffers
      PFD_MAIN_PLANE,			/* main layer */
      0,	
      0, 0, 0
   };
   int  selected_pf;
   
   if ( ( selected_pf = ChoosePixelFormat( hDC, &pfd ) ) == 0 )
   {
      MessageBox( 0, "Failed to find acceptable pixel format", "GL Sphere Error", MB_ICONERROR | MB_OK );
      return FALSE;
   }
   
   if ( !SetPixelFormat( hDC, selected_pf, &pfd) )
   {
      MessageBox( 0, "Failed to SetPixelFormat", "GL Sphere Error", MB_ICONERROR | MB_OK );
      return FALSE;
   }
   return TRUE;
}

static void SphereSetDefaults( void )
{
   // setup a default color
   glColor3f( 1.0F, 1.0F, 1.0F );
   glClearColor( 0.0F, 0.0F, 1.0F, 1.0F );

#if 0
   glClearDepth( 1.0F );
   glDepthFunc( GL_LEQUAL );
   glEnable( GL_DEPTH_TEST );
   glDepthMask( GL_TRUE );
#endif

   // setup perspective
   glMatrixMode( GL_PROJECTION );
   glFrustum( -0.5, 0.5, -0.5, 0.5, 1.0, 1000.0 );

   // setup viewer
   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity();
   glTranslatef( 0.0F, 0.0F, -3.0F );

   // setup Z-buffering
   // setup shade model
   glShadeModel( GL_SMOOTH );

   // setup lighting
   if ( !s_lighting )
   {
      glDisable( GL_LIGHTING );
      glDisable( GL_LIGHT0 );
   }
   else
   {
      GLfloat light0Pos[4] = { 0.70F, 0.70F, 1.25F, 0.00F };
      GLfloat matAmb[4] = { 0.01F, 0.01F, 0.01F, 1.00F };
      GLfloat matDiff[4] = { 0.65F, 0.05F, 0.20F, 0.60F };
      GLfloat matSpec[4] = { 0.50F, 0.50F, 0.50F, 1.00F };
      GLfloat matShine = 20.00F;

      glEnable( GL_LIGHTING );
      glEnable( GL_LIGHT0 );
      glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
      glMaterialfv(GL_FRONT, GL_AMBIENT, matAmb);
      glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiff);
      glMaterialfv(GL_FRONT, GL_SPECULAR, matSpec);
      glMaterialf(GL_FRONT, GL_SHININESS, matShine);
   }

   // setup cull mode
   glEnable( GL_CULL_FACE );

#ifdef GL_SGI_cull_vertex
   if ( s_vcull )
   {
       GLfloat eyeDir[] = { 0.0F, 0.0F, 1.0F, 0.0F };

       glEnable(GL_CULL_VERTEX_SGI);

       if ( CullParameterfvSGI )
           CullParameterfvSGI( GL_CULL_VERTEX_EYE_POSITION_SGI, eyeDir );
   }
   else
   {
       glDisable(GL_CULL_VERTEX_SGI);
   }
#endif

   // setup dither
   glEnable( GL_DITHER );

   glClear( GL_COLOR_BUFFER_BIT );
   SwapBuffers( s_hDC );
}

static BOOL SphereCreateGeometry( int rows, int cols, float scale )
{
   float   cap_offset = 10.0F;

   float   x_delta = ( 360.0F - 2.0f * cap_offset ) / cols;
   float   y_delta = ( 180.0F - 2.0f * cap_offset ) / rows;
   int     row, col;
   int     num_vertex_rows = rows + 1;
   int     num_vertex_cols = cols;

   float  *spa, *sna, *sia;

   s_sphere_points  = ( float ** ) AllocAlignedX( sizeof( float ) * num_vertex_rows, BYTES_PER_CACHELINE );
   s_sphere_normals = ( float ** ) AllocAlignedX( sizeof( float ) * num_vertex_rows, BYTES_PER_CACHELINE );

   spa = s_sphere_point_array  = ( float * ) AllocAlignedX( sizeof( float ) * ELEMENTS_PER_VERTEX * num_vertex_rows * num_vertex_cols, BYTES_PER_CACHELINE );
   sna = s_sphere_normal_array = ( float * ) AllocAlignedX( sizeof( float ) * ELEMENTS_PER_NORMAL * num_vertex_rows * num_vertex_cols, BYTES_PER_CACHELINE );

   if ( !s_sphere_points || !s_sphere_normals || !s_sphere_point_array || !s_sphere_normal_array )
      return FALSE;

   for ( row = 0; row < num_vertex_rows; row++ )
   {
      s_sphere_points[row]  = ( float * ) AllocAlignedX( sizeof( float ) * num_vertex_cols * ELEMENTS_PER_VERTEX, BYTES_PER_CACHELINE );
      s_sphere_normals[row] = ( float * ) AllocAlignedX( sizeof( float ) * num_vertex_cols * ELEMENTS_PER_NORMAL, BYTES_PER_CACHELINE );
   }

   for ( row = 0; row < num_vertex_rows; row++ )
   {
      float rowangle;

      rowangle = row*y_delta + cap_offset;

      for ( col = 0; col < num_vertex_cols; col++ )
      {
         float colangle;

         colangle = col*x_delta;

         spa[0] = s_sphere_points[row][col*ELEMENTS_PER_VERTEX+0] = ( float ) ( sin( DEG2RAD( colangle ) ) * sin ( DEG2RAD( rowangle ) ) * scale );
         spa[1] = s_sphere_points[row][col*ELEMENTS_PER_VERTEX+1] = ( float ) ( sin( DEG2RAD( rowangle + 270.0F ) ) * scale );
         spa[2] = s_sphere_points[row][col*ELEMENTS_PER_VERTEX+2] = ( float ) ( sin( DEG2RAD( colangle + 270.0F ) ) * sin ( DEG2RAD( rowangle ) ) * scale );
#if ( ELEMENTS_PER_VERTEX == 4 )
         spa[3] = s_sphere_points[row][col*ELEMENTS_PER_VERTEX+3] = 1.0F;
#endif

         spa += ELEMENTS_PER_VERTEX;

         sna[0] = s_sphere_normals[row][col*ELEMENTS_PER_NORMAL+0] = s_sphere_points[row][col*ELEMENTS_PER_VERTEX+0];
         sna[1] = s_sphere_normals[row][col*ELEMENTS_PER_NORMAL+1] = s_sphere_points[row][col*ELEMENTS_PER_VERTEX+1];
         sna[2] = s_sphere_normals[row][col*ELEMENTS_PER_NORMAL+2] = s_sphere_points[row][col*ELEMENTS_PER_VERTEX+2];
#if ( ELEMENTS_PER_NORMAL == 4 )
         sna[3] = s_sphere_normals[row][col*ELEMENTS_PER_NORMAL+3] = 1.0F;
#endif

         SphereNormalizeVector( &s_sphere_normals[row][col*ELEMENTS_PER_NORMAL] );
         SphereNormalizeVector( sna );

         sna += ELEMENTS_PER_NORMAL;
      }
   }

   /*
   ** create the draw arrays data
   */
   s_sphere_point_draw_array  = ( float ** ) AllocAlignedX( sizeof( float * ) * s_num_rows, BYTES_PER_CACHELINE );
   s_sphere_normal_draw_array = ( float ** ) AllocAlignedX( sizeof( float * ) * s_num_rows, BYTES_PER_CACHELINE );

   for ( row = 0; row < s_num_rows; row++ )
   {
      s_sphere_point_draw_array[row]  = ( float * ) AllocAlignedX( sizeof( float ) * ELEMENTS_PER_VERTEX * ( s_num_cols + 1 ) * 2, BYTES_PER_CACHELINE );
      s_sphere_normal_draw_array[row] = ( float * ) AllocAlignedX( sizeof( float ) * ELEMENTS_PER_NORMAL * ( s_num_cols + 1 ) * 2, BYTES_PER_CACHELINE );
   }
   for ( row = 0; row < s_num_rows; row++ )
   {
      for ( col = 0; col < s_num_cols; col++ )
      {
         float *pdst, *psrc;

         pdst = &s_sphere_point_draw_array[row][col*2*ELEMENTS_PER_VERTEX];
         psrc = &s_sphere_points[row][col*ELEMENTS_PER_VERTEX];
         memcpy( pdst, psrc, sizeof( float ) * ELEMENTS_PER_VERTEX );

         pdst = &s_sphere_point_draw_array[row][(col*2+1)*ELEMENTS_PER_VERTEX];
         psrc = &s_sphere_points[row+1][col*ELEMENTS_PER_VERTEX];
         memcpy( pdst, psrc, sizeof( float ) * ELEMENTS_PER_VERTEX );

         pdst = &s_sphere_normal_draw_array[row][col*2*ELEMENTS_PER_NORMAL];
         psrc = &s_sphere_normals[row][col*ELEMENTS_PER_NORMAL];
         memcpy( pdst, psrc, sizeof( float ) * ELEMENTS_PER_NORMAL );

         pdst = &s_sphere_normal_draw_array[row][(col*2+1)*ELEMENTS_PER_NORMAL];
         psrc = &s_sphere_normals[row+1][col*ELEMENTS_PER_NORMAL];
         memcpy( pdst, psrc, sizeof( float ) * ELEMENTS_PER_NORMAL );
      }
      memcpy( &s_sphere_point_draw_array[row][s_num_cols*2*ELEMENTS_PER_VERTEX],      &s_sphere_points[row][0],    sizeof( float ) * ELEMENTS_PER_VERTEX );
      memcpy( &s_sphere_point_draw_array[row][(s_num_cols*2+1)*ELEMENTS_PER_VERTEX],  &s_sphere_points[row+1][0],  sizeof( float ) * ELEMENTS_PER_VERTEX );
      memcpy( &s_sphere_normal_draw_array[row][s_num_cols*2*ELEMENTS_PER_NORMAL],     &s_sphere_normals[row][0],   sizeof( float ) * ELEMENTS_PER_NORMAL );
      memcpy( &s_sphere_normal_draw_array[row][(s_num_cols*2+1)*ELEMENTS_PER_NORMAL], &s_sphere_normals[row+1][0], sizeof( float ) * ELEMENTS_PER_NORMAL );
   }

   /*
   ** create the draw elements data
   */
   s_sphere_elements = ( unsigned int ** ) malloc( sizeof( unsigned int * ) * s_num_rows );
   for ( row = 0; row < s_num_rows; row++ )
   {
      s_sphere_elements[row] = ( unsigned int * ) malloc( sizeof( unsigned int ) * ( 2 * s_num_cols + 2 ) );
   }
   for ( row = 0; row < s_num_rows; row++ )
   {
      for ( col = 0; col < s_num_cols*2; col += 2 )
      {
         s_sphere_elements[row][col]   = row * s_num_cols + col/2;
         s_sphere_elements[row][col+1] = ( row + 1 ) * s_num_cols + col/2;
      }

      s_sphere_elements[row][s_num_cols*2]   = row * s_num_cols;
      s_sphere_elements[row][s_num_cols*2+1] = ( row + 1 ) * s_num_cols;
   }
   sia = s_sphere_interleaved_array = ( float * ) malloc( sizeof( float ) * ELEMENTS_PER_VERTEX * ELEMENTS_PER_NORMAL * ( s_num_rows + 1 ) * ( s_num_cols + 1 ) );
   memset( s_sphere_interleaved_array, 0, sizeof( float ) * ELEMENTS_PER_VERTEX * ELEMENTS_PER_NORMAL * ( s_num_rows + 1 ) * ( s_num_cols + 1 ) );

   for ( row = 0; row < num_vertex_rows; row++ )
   {
      for ( col = 0; col < s_num_cols; col++ )
      {
         memcpy( sia, &s_sphere_normals[row][col*ELEMENTS_PER_NORMAL], sizeof( float ) * ELEMENTS_PER_NORMAL );

         sia += ELEMENTS_PER_NORMAL;

         memcpy( sia, &s_sphere_points[row][col*ELEMENTS_PER_VERTEX], sizeof( float ) * ELEMENTS_PER_VERTEX );

         sia += ELEMENTS_PER_VERTEX;
      }
   }

   return TRUE;
}

static void SphereRedraw( void )
{
   int row, col;

   if ( !s_benchmark )
      glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   glRotatef( 1.0f, 0.0F, 1.0F, 0.0F );
   glRotatef( 1.0f, 1.0F, 0.0F, 0.0F );

   if ( s_draw_method == DRAW_VERTEX3FV )
   {
      for ( row = 0; row < s_num_rows; row++ )
      {
         glBegin( GL_TRIANGLE_STRIP );
         for ( col = 0; col < s_num_cols; col++ )
         {
            glNormal3fv( &s_sphere_normals[row][col*ELEMENTS_PER_NORMAL] );
            glVertex3fv( &s_sphere_points[row][col*ELEMENTS_PER_VERTEX] );
            glNormal3fv( &s_sphere_normals[row+1][col*ELEMENTS_PER_NORMAL] );
            glVertex3fv( &s_sphere_points[row+1][col*ELEMENTS_PER_VERTEX] );
         }
         glNormal3fv( &s_sphere_normals[row][0] );
         glVertex3fv( &s_sphere_points[row][0] );
         glNormal3fv( &s_sphere_normals[row+1][0] );
         glVertex3fv( &s_sphere_points[row+1][0] );
         
         glEnd();
      }
   }
   else if ( s_draw_method == DRAW_DRAW_ARRAYS )
   {
      for ( row = 0; row < s_num_rows; row++ )
      {
         glVertexPointer( ELEMENTS_PER_VERTEX, GL_FLOAT, ELEMENTS_PER_VERTEX * sizeof( float ), ( const GLvoid * )  s_sphere_point_draw_array[row] );
         glNormalPointer( GL_FLOAT, ELEMENTS_PER_NORMAL * sizeof( float ), ( const GLfloat * ) s_sphere_normal_draw_array[row] );
         glDrawArrays( GL_TRIANGLE_STRIP, 0, (s_num_cols+1)*2 );
      }
   }
   else if ( s_draw_method == DRAW_ARRAY_ELEMENT )
   {
      for ( row = 0; row < s_num_rows; row++ )
      {
         glBegin( GL_TRIANGLE_STRIP );

         for ( col = 0; col < s_num_cols; col++ )
         {
            glArrayElement( col + row*s_num_cols );
            glArrayElement( col + (row+1)*s_num_cols );
         }

         glArrayElement( row*s_num_cols );
         glArrayElement( (row+1)*s_num_cols );

         glEnd();
      }
   }
   else if ( s_draw_method == DRAW_DRAW_ELEMENTS )
   {
      glInterleavedArrays( GL_N3F_V3F, ( ELEMENTS_PER_VERTEX + ELEMENTS_PER_NORMAL ) * sizeof( float ), s_sphere_interleaved_array );

      if ( LockArraysSGI && s_lock_arrays )
      {
         LockArraysSGI( 0, ( s_num_rows + 1 ) * ( s_num_cols + 1 ) );
      }

      for ( row = 0; row < s_num_rows; row++ )
      {
         glDrawElements( GL_TRIANGLE_STRIP, (s_num_cols+1)*2, GL_UNSIGNED_INT, s_sphere_elements[row] );
      }

      if ( UnlockArraysSGI && s_lock_arrays )
      {
         UnlockArraysSGI();
      }
   }

   s_vertices_processed  += ( ( s_num_rows + 1 ) * ( s_num_cols ) );
   s_triangles_processed += ( s_num_rows * s_num_cols );

   if ( !s_benchmark )
     SwapBuffers( s_hDC );
}

static void SphereNormalizeVector( float v[3] )
{
   float m = 1.0F / ( float ) ( sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] ) );

   v[0] *= m;
   v[1] *= m;
   v[2] *= m;
}

static void *AllocAlignedX( int num_bytes, int byte_alignment )
{
   void *buf = malloc( num_bytes + byte_alignment );
   unsigned long q = ( unsigned long ) buf;

   q += byte_alignment;

   q &= ~( byte_alignment - 1 );

   return ( void * ) q;
}

static void SphereHelp( void )
{
   char superbuffer[1000]=
      "-?\t\tthis help screen\n"
      "-arrayelements\tuse glArrayElement\n"
      "-drawelements\tuse glDrawElements\n"
      "-drawarrays\tuse glDrawArrays\n"
      "-tristrip\t\tuse triangle strips w/ glVertex3fv\n"
      "-vcull\t\tuse SGI's vertex culling extension\n"
      "-lockarrays\tuse SGI's lock arrays extension when doing drawelements\n\n"
      "-cols:val\t\tspecify number of vertical (longitudinal) bands in sphere\n"
      "-rows:val\t\tspecify number of horizontal (latitudinal) bands in sphere\n\n"
      "-width:val\t\twidth of window, in pixels (default is 320)\n"
      "-height:val\t\theight of window, in pixels (default is 240)\n\n"
      "-benchmark\trun as benchmark and immediately quit when done\n"
      "-remote\t\tdon't print performance data\n";

   MessageBox( 0, superbuffer, "Sphere, Copyright (C) 1997 Silicon Graphics, Inc.", MB_OK | MB_ICONINFORMATION );
}

static void SphereSetupPalette( HDC hDC )
{
    PIXELFORMATDESCRIPTOR  pfd;
    LOGPALETTE            *pPal;
    int                    paletteSize;
    int                    pixelFormat = GetPixelFormat(hDC);
    
    DescribePixelFormat( hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    if (!(pfd.dwFlags & PFD_NEED_PALETTE || pfd.iPixelType == PFD_TYPE_COLORINDEX))
        return;
    
    paletteSize = 1 << pfd.cColorBits;

    pPal                = (LOGPALETTE * ) malloc( sizeof(LOGPALETTE) + paletteSize * sizeof(PALETTEENTRY) );
    pPal->palVersion    = 0x300;
    pPal->palNumEntries = paletteSize;
    
    /* start with a copy of the current system palette */
    GetSystemPaletteEntries( hDC, 0, paletteSize, &pPal->palPalEntry[0] );
    
    if ( pfd.iPixelType == PFD_TYPE_RGBA )
    {
        /* fill in an RGBA color palette */
        int redMask = (1 << pfd.cRedBits) - 1;
        int greenMask = (1 << pfd.cGreenBits) - 1;
        int blueMask = (1 << pfd.cBlueBits) - 1;
        int i;
        
        for (i=0; i<paletteSize; ++i) 
        {
            pPal->palPalEntry[i].peRed =
                (((i >> pfd.cRedShift) & redMask) * 255) / redMask;
            pPal->palPalEntry[i].peGreen =
                (((i >> pfd.cGreenShift) & greenMask) * 255) / greenMask;
            pPal->palPalEntry[i].peBlue =
                (((i >> pfd.cBlueShift) & blueMask) * 255) / blueMask;
            pPal->palPalEntry[i].peFlags = 0;
        }
    } 
    else 
    {
        /* fill in a Color Index ramp color palette */
        int numRamps = NUM_COLORS;
        int rampSize = (paletteSize - 20) / numRamps;
        int extra = (paletteSize - 20) - (numRamps * rampSize);
        int i, r;
        
        for (r=0; r<numRamps; ++r) 
        {
            int rampBase = r * rampSize + 10;
            PALETTEENTRY *pe = &pPal->palPalEntry[rampBase];
            int diffSize = (int) (rampSize * s_colors[r].ratio);
            int specSize = rampSize - diffSize;
            
            for (i=0; i<rampSize; ++i) 
            {
                GLfloat *c0, *c1;
                GLint a;
                
                if (i < diffSize) 
                {
                    c0 = s_colors[r].amb;
                    c1 = s_colors[r].diff;
                    a = (i * 255) / (diffSize - 1);
                } 
                else 
                {
                    c0 = s_colors[r].diff;
                    c1 = s_colors[r].spec;
                    a = ((i - diffSize) * 255) / (specSize - 1);
                }
                
                pe[i].peRed = (BYTE) (a * (c1[0] - c0[0]) + 255 * c0[0]);
                pe[i].peGreen = (BYTE) (a * (c1[1] - c0[1]) + 255 * c0[1]);
                pe[i].peBlue = (BYTE) (a * (c1[2] - c0[2]) + 255 * c0[2]);
                pe[i].peFlags = PC_NOCOLLAPSE;
            }
            
            s_colors[r].indexes[0] = rampBase;
            s_colors[r].indexes[1] = rampBase + (diffSize-1);
            s_colors[r].indexes[2] = rampBase + (rampSize-1);
        }
        s_lit_tex_indexes[0] = 0;
        s_lit_tex_indexes[1] = (GLint)(rampSize*s_colors[0].ratio)-1;
        s_lit_tex_indexes[2] = rampSize-1;
        
        for (i=0; i<extra; ++i) 
        {
            int index = numRamps*rampSize+10+i;
            PALETTEENTRY *pe = &pPal->palPalEntry[index];
            
            pe->peRed = (BYTE) 0;
            pe->peGreen = (BYTE) 0;
            pe->peBlue = (BYTE) 0;
            pe->peFlags = PC_NOCOLLAPSE;
        }
    }
    
    s_hPalette = CreatePalette( pPal );
    free( pPal );
    
    if ( s_hPalette )
    {
        SelectPalette( hDC, s_hPalette, FALSE );
        RealizePalette( hDC );
    }
}


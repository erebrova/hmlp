#include <hmlp_device.hpp>
#include <hmlp_runtime.hpp>

namespace hmlp
{

CacheLine::CacheLine() {};

void CacheLine::Setup( hmlp::Device *device, size_t line_size )
{
  this->status = CACHE_CLEAN;
  this->line_size = line_size;
  this->ptr_d = (char*)device->malloc( line_size );
};

bool CacheLine::isClean()
{
  return ( status == CACHE_CLEAN );
};

void CacheLine::Bind( void *ptr_h )
{
  this->ptr_h = ptr_h;
};


bool CacheLine::isCache( void *ptr_h, size_t size )
{
  if ( size > line_size )
  {
    printf( "Cache line %lu request %lu\n", line_size, size );
    return false;
  }
  else
  {
    return (this->ptr_h == ptr_h);
  }
};

char *CacheLine::device_data()
{
  return ptr_d;
};



Cache::Cache() {};

void Cache::Setup( hmlp::Device *device )
{
  for ( int line_id = 0; line_id < 8; line_id ++ )
  {
    line[ line_id ].Setup( device, 4096 * 4096 * 8 );
  }
};

CacheLine *Cache::Read( size_t size )
{
  int line_id = fifo;
  fifo = ( fifo + 1 ) % 8;
  if ( !line[ line_id ].isClean() )
  {
    printf( "The cache line is not clean\n" );
    exit( 1 );
  }
  else
  {
    //printf( "line_id %d\n", line_id );
  }
  return &(line[ line_id ]);
};






/**
 *  @brief Device implementation
 */
Device::Device()
{
  name = std::string( "Host CPU" );
  devicetype = hmlp::DeviceType::HOST;
};


CacheLine *Device::getline( size_t size ) 
{
  return cache.Read( size );
};

void Device::prefetchd2h( void *ptr_h, void *ptr_d, size_t size, int stream_id ) {};

void Device::prefetchh2d( void *ptr_d, void *ptr_h, size_t size, int stream_id ) {};

void Device::wait( int stream_id ) {};

void *Device::malloc( size_t size ) { return NULL; };

void Device::malloc( void *ptr_d, size_t size ) {};

size_t Device::get_memory_left() { return 0; };

void Device::free( void *ptr_d, size_t size ) {};


}; // end namespace hmlp

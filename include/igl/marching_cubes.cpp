/*===========================================================================*\
 *                                                                           *
 *                                IsoEx                                      *
 *        Copyright (C) 2002 by Computer Graphics Group, RWTH Aachen         *
 *                         www.rwth-graphics.de                              *
 *                                                                           *
 *---------------------------------------------------------------------------* 
 *                                                                           *
 *                                License                                    *
 *                                                                           *
 *  This library is free software; you can redistribute it and/or modify it  *
 *  under the terms of the GNU Library General Public License as published   *
 *  by the Free Software Foundation, version 2.                              *
 *                                                                           *
 *  This library is distributed in the hope that it will be useful, but      *
 *  WITHOUT ANY WARRANTY; without even the implied warranty of               *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *  Library General Public License for more details.                         *
 *                                                                           *
 *  You should have received a copy of the GNU Library General Public        *
 *  License along with this library; if not, write to the Free Software      *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                *
 *                                                                           *
 \*===========================================================================*/

#include "marching_cubes.h"
#include <map>

extern int edgeTable[256];
extern int triTable[256][2][17];
extern int polyTable[8][16];

class EdgeKey 
{
public:
  
  EdgeKey(unsigned i0, unsigned i1) {
    if (i0 < i1)  { i0_ = i0;  i1_ = i1; }
    else            { i0_ = i1;  i1_ = i0; }
  }
  
  bool operator<(const EdgeKey& _rhs) const 
  {
    if (i0_ != _rhs.i0_)
      return (i0_ < _rhs.i0_);
    else
      return (i1_ < _rhs.i1_);
  }
  
private:
  unsigned i0_, i1_;
};


template <typename DerivedVal, typename DerivedV, typename DerivedF>
class MarchingCubes
{
  typedef Eigen::PlainObjectBase<DerivedV> PointMatrixType;
  typedef Eigen::PlainObjectBase<DerivedF> FaceMatrixType;
  typedef std::map<EdgeKey, unsigned>  MyMap;
  typedef typename MyMap::const_iterator   MyMapIterator;
  
public:
  MarchingCubes(const Eigen::PlainObjectBase<DerivedVal> &values,
                const const Eigen::PlainObjectBase<DerivedV> &points,
                const unsigned x_res,
                const unsigned y_res,
                const unsigned z_res,
                PointMatrixType &vertices,
                FaceMatrixType &faces)
  {
    
    if(x_res <2 || y_res<2 ||z_res<2)
      return;
    faces.resize(10000,3);
    int num_faces = 0;
    
    vertices.resize(10000,3);
    int num_vertices = 0;
    
    
    unsigned n_cubes  = (x_res-1) * (y_res-1) * (z_res-1);
    assert(unsigned(points.rows()) == x_res * y_res * z_res);
    
    unsigned int         offsets_[8];
    offsets_[0] = 0;
    offsets_[1] = 1;
    offsets_[2] = 1 + x_res;
    offsets_[3] =     x_res;
    offsets_[4] =             x_res*y_res;
    offsets_[5] = 1         + x_res*y_res;
    offsets_[6] = 1 + x_res + x_res*y_res;
    offsets_[7] =     x_res + x_res*y_res;
    
    for (unsigned cube_it =0 ; cube_it < n_cubes; ++cube_it)
    {
      
      unsigned         corner[8];
      typename DerivedF::Scalar samples[12];
      unsigned char    cubetype(0);
      unsigned int     i;
      
      
      // get point indices of corner vertices
      for (i=0; i<8; ++i)
      {
        // get cube coordinates
        unsigned int _idx = cube_it;
        unsigned int X(x_res-1), Y(y_res-1);
        unsigned int x = _idx % X;  _idx /= X;
        unsigned int y = _idx % Y;  _idx /= Y;
        unsigned int z = _idx;
        
        // transform to point coordinates
        _idx = x + y*x_res + z*x_res*y_res;
        
        // add offset
        corner[i] = _idx + offsets_[i];
      }
      
      
      // determine cube type
      for (i=0; i<8; ++i)
        if (values[corner[i]] > 0.0)
          cubetype |= (1<<i);
      
      
      // trivial reject ?
      if (cubetype == 0 || cubetype == 255)
        continue;
      
      
      // compute samples on cube's edges
      if (edgeTable[cubetype]&1)  
        samples[0]  = add_vertex(values, points, corner[0], corner[1], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&2)  
        samples[1]  = add_vertex(values, points, corner[1], corner[2], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&4)  
        samples[2]  = add_vertex(values, points, corner[3], corner[2], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&8)  
        samples[3]  = add_vertex(values, points, corner[0], corner[3], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&16) 
        samples[4]  = add_vertex(values, points, corner[4], corner[5], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&32)  
        samples[5]  = add_vertex(values, points, corner[5], corner[6], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&64)  
        samples[6]  = add_vertex(values, points, corner[7], corner[6], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&128) 
        samples[7]  = add_vertex(values, points, corner[4], corner[7], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&256) 
        samples[8]  = add_vertex(values, points, corner[0], corner[4], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&512) 
        samples[9]  = add_vertex(values, points, corner[1], corner[5], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&1024)
        samples[10] = add_vertex(values, points, corner[2], corner[6], vertices, num_vertices, edge2vertex);
      if (edgeTable[cubetype]&2048)
        samples[11] = add_vertex(values, points, corner[3], corner[7], vertices, num_vertices, edge2vertex);
      
      
      
      // connect samples by triangles
      for (i=0; triTable[cubetype][0][i] != -1; i+=3 )
      {
        num_faces++;
        if (num_faces > faces.rows())
          faces.conservativeResize(faces.rows()+10000, Eigen::NoChange);
        
        faces.row(num_faces-1) << 
        samples[triTable[cubetype][0][i  ]],
        samples[triTable[cubetype][0][i+1]],
        samples[triTable[cubetype][0][i+2]];
        
      }
      
    }
    
    vertices.conservativeResize(num_vertices, Eigen::NoChange);
    faces.conservativeResize(num_faces, Eigen::NoChange);
    
  };
  
  static typename DerivedF::Scalar  add_vertex(const Eigen::PlainObjectBase<DerivedVal> &values,
                                               const const Eigen::PlainObjectBase<DerivedV> &points,
//                                               const Eigen::Matrix<typename DerivedV::Scalar, Eigen::Dynamic, 1> &values,
//                                               const Eigen::Matrix<typename DerivedV::Scalar, Eigen::Dynamic, 3> &points,
                                               unsigned int i0,
                                               unsigned int i1,
                                               PointMatrixType &vertices,
                                               int &num_vertices,
                                               MyMap &edge2vertex)
  {
    // find vertex if it has been computed already
    MyMapIterator it = edge2vertex.find(EdgeKey(i0, i1));
    if (it != edge2vertex.end()) 
      return it->second;
    ;
    
    // generate new vertex
    const Eigen::Matrix<typename DerivedV::Scalar, 1, 3> & p0 = points.row(i0);
    const Eigen::Matrix<typename DerivedV::Scalar, 1, 3> & p1 = points.row(i1);
    
    double s0 = fabs(values[i0]);
    double s1 = fabs(values[i1]);
    double t  = s0 / (s0+s1);
    
    
    num_vertices++;
    if (num_vertices > vertices.rows())
      vertices.conservativeResize(vertices.rows()+10000, Eigen::NoChange);
    
    vertices.row(num_vertices-1)  = (1.0f-t)*p0 + t*p1;
    edge2vertex[EdgeKey(i0, i1)] = num_vertices-1;
    
    return num_vertices-1;
  }
  ;
  
  // maps an edge to the sample vertex generated on it
  MyMap  edge2vertex;
};


template <typename DerivedVal, typename DerivedV, typename DerivedF>
IGL_INLINE void igl::marching_cubes(const Eigen::PlainObjectBase<DerivedVal> &values,
                                    const const Eigen::PlainObjectBase<DerivedV> &points,
                                    const unsigned x_res,
                                    const unsigned y_res,
                                    const unsigned z_res,
                                    Eigen::PlainObjectBase<DerivedV> &vertices,
                                    Eigen::PlainObjectBase<DerivedF> &faces)
{
  MarchingCubes<DerivedVal, DerivedV, DerivedF> mc(values,
                                       points, 
                                       x_res, 
                                       y_res, 
                                       z_res, 
                                       vertices, 
                                       faces);
}
#ifdef IGL_STATIC_LIBRARY
// Explicit template specialization
//template void igl::marching_cubes<Eigen::Matrix<float, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 3, 0, -1, 3> >(Eigen::Matrix<Eigen::Matrix<float, -1, 3, 0, -1, 3>::Scalar, -1, 1, 0, -1, 1> const&, Eigen::Matrix<Eigen::Matrix<float, -1, 3, 0, -1, 3>::Scalar, -1, 3, 0, -1, 3> const&, unsigned int, unsigned int, unsigned int, Eigen::PlainObjectBase<Eigen::Matrix<float, -1, 3, 0, -1, 3> >&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 3, 0, -1, 3> >&);
//template void igl::marching_cubes<Eigen::Matrix<double, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 3, 0, -1, 3> >(Eigen::Matrix<Eigen::Matrix<double, -1, 3, 0, -1, 3>::Scalar, -1, 1, 0, -1, 1> const&, Eigen::Matrix<Eigen::Matrix<double, -1, 3, 0, -1, 3>::Scalar, -1, 3, 0, -1, 3> const&, unsigned int, unsigned int, unsigned int, Eigen::PlainObjectBase<Eigen::Matrix<double, -1, 3, 0, -1, 3> >&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 3, 0, -1, 3> >&);
//template void igl::marching_cubes<Eigen::Matrix<double, -1, 3, 0, -1, 3>, Eigen::Matrix<unsigned int, -1, 3, 0, -1, 3> >(Eigen::Matrix<Eigen::Matrix<double, -1, 3, 0, -1, 3>::Scalar, -1, 1, 0, -1, 1> const&, Eigen::Matrix<Eigen::Matrix<double, -1, 3, 0, -1, 3>::Scalar, -1, 3, 0, -1, 3> const&, unsigned int, unsigned int, unsigned int, Eigen::PlainObjectBase<Eigen::Matrix<double, -1, 3, 0, -1, 3> >&, Eigen::PlainObjectBase<Eigen::Matrix<unsigned int, -1, 3, 0, -1, 3> >&);
//template void igl::marching_cubes<Eigen::Matrix<float, -1, 3, 0, -1, 3>, Eigen::Matrix<unsigned int, -1, 3, 0, -1, 3> >(Eigen::Matrix<Eigen::Matrix<float, -1, 3, 0, -1, 3>::Scalar, -1, 1, 0, -1, 1> const&, Eigen::Matrix<Eigen::Matrix<float, -1, 3, 0, -1, 3>::Scalar, -1, 3, 0, -1, 3> const&, unsigned int, unsigned int, unsigned int, Eigen::PlainObjectBase<Eigen::Matrix<float, -1, 3, 0, -1, 3> >&, Eigen::PlainObjectBase<Eigen::Matrix<unsigned int, -1, 3, 0, -1, 3> >&);
#endif

#ifndef Interpolators__1D_SplineInterpolator_hpp
#define Interpolators__1D_SplineInterpolator_hpp

#include "InterpolatorBase.hpp"

namespace _1D {


/** @class 
  * @brief An implementation of the spline interpolation algorithm (see the wikipedia page on "Spline interpolation" (https://en.wikipedia.org/wiki/Spline_interpolation).
  * @author C.D. Clark III
  *
  * This class does *not* do extrapolation.
  */
template<class Real>
class SplineInterpolator : public InterpolatorBase<Real>
{

  public:
    typedef typename InterpolatorBase<Real>::VectorType VectorType;
    typedef typename InterpolatorBase<Real>::MapType MapType;

    using InterpolatorBase<Real>::operator();
    virtual Real operator()( Real x ) const;
    virtual Real derivative( Real x ) const;
    virtual Real integral( Real a, Real b) const;

    virtual void setData( size_t _n, Real *x, Real *y, bool deep_copy = true );
    virtual void setData( std::vector<Real> &x, std::vector<Real> &y, bool deep_copy = true );
    virtual void setData( VectorType  &x, VectorType &y, bool deep_copy = true );

  protected:

    // Interpolation coefficients
    VectorType a,b;

    void calcCoefficients();

};





template<class Real>
void
SplineInterpolator<Real>::setData( size_t n, Real *x, Real *y, bool deep_copy )
{
  InterpolatorBase<Real>::setData( n, x, y, deep_copy );
  calcCoefficients();
}

template<class Real>
void
SplineInterpolator<Real>::setData( std::vector<Real> &x, std::vector<Real> &y, bool deep_copy )
{
  InterpolatorBase<Real>::setData( x, y, deep_copy );
  calcCoefficients();
}

template<class Real>
void
SplineInterpolator<Real>::setData( VectorType  &x, VectorType &y, bool deep_copy )
{
  InterpolatorBase<Real>::setData( x, y, deep_copy );
  calcCoefficients();
}










template<class Real>
Real
SplineInterpolator<Real>::operator()( Real x ) const
{
  InterpolatorBase<Real>::checkData();

  const VectorType &X = *(this->xv);
  const VectorType &Y = *(this->yv);

  // find the index that is just to the right of the x
  int i = Utils::index_first_ge( x, X, 1);

  // don't extrapolate at all
  if( i == 0 || i == X.size())
    return 0;

  // See the wikipedia page on "Spline interpolation" (https://en.wikipedia.org/wiki/Spline_interpolation)
  // for a derivation this interpolation.
  Real t = ( x - X(i-1) ) / ( X(i) - X(i-1) );
  Real q = ( 1 - t ) * Y(i-1) + t * Y(i) + t*(1-t)*(a[i-1]*(1-t)+b[i-1]*t);
  
  return q;
}

template<typename Real>
Real
SplineInterpolator<Real>::derivative( Real x ) const
{
  const VectorType &X = *(this->xv);
  const VectorType &Y = *(this->yv);

  // find the index that is just to the right of the x
  int i = Utils::index_first_gt( x, X, 1);

  // don't extrapolate at all
  if( i == 0 || i == X.size())
    return 0;

  //this should be the same t as in the regular interpolation case
  Real t = ( x - X(i-1) ) / ( X(i) - X(i-1) );

  Real qprime = ( Y(i) - Y(i-1) )/( X(i)-X(i-1) ) + ( 1 - 2*t )*( a[i-1]*(1-t) + b[i-1]*t )/( X(i) - X(i-1))
                  + t*(1-t)*(b[i-1]-a[i-1])/(X(i)-X(i-1)) ;

  return qprime;
}



template<class Real>
Real
SplineInterpolator<Real>::integral( Real _a, Real _b ) const
{
  if( this->xv->size() < 1 )
    return 0;

  const VectorType &X = *(this->xv);
  const VectorType &Y = *(this->yv);

  // allow b to be less than a
  int sign = 1;
  if( _a > _b )
  {
    std::swap( _a, _b );
    sign = -1;
  }
  // no extrapolation
  _a = std::max( _a, X[0] );
  _b = std::min( _b, X[X.size()-1] );

  // find the indexes that is just to the right of a and b
  int ai = Utils::index_first_gt( _a, X, 1 );
  int bi = Utils::index_first_gt( _b, X, 1 );

  /**
   *
   * We can integrate the function directly using its cubic spline representation.
   *
   * from wikipedia:
   *
   * q(x) = ( 1 - t )*y_1 + t*y_2 + t*( 1 - t )( a*(1 - t) + b*t )
   * 
   * t = (x - x_1) / (x_2 - x_1)
   *
   * I = \int_a^b q(x) dx = \int_a^b ( 1 - t )*y_1 + t*y_2 + t*( 1 - t )( a*(1 - t) + b*t ) dx
   *
   * variable substitution: x -> t
   *
   * dt = dx / (x_2 - x_1)
   * t_a = (a - x_1) / (x_2 - x_1)
   * t_b = (b - x_1) / (x_2 - x_1)
   *
   * I = (x_2 - x_1) \int_t_a^t_b ( 1 - t )*y_1 + t*y_2 + t*( 1 - t )( a*(1 - t) + b*t ) dt
   *
   *   = (x_2 - x_1) [ ( t - t^2/2 )*y_1 + t^2/2*y_2 + a*(t^2 - 2*t^3/3 + t^4/4) + b*(t^3/3 - t^4/4) ] |_t_a^t_b
   *
   * if we integrate over the entire element, i.e. x -> [x1,x2], then we will have
   * t_a = 0, t_b = 1. This gives
   *
   * I = (x_2 - x_1) [ ( 1 - 1/2 )*y_1 + 1/2*y_2 + a*(1 - 2/3 + 1/4) + b*(1/3 - 1/4) ]
   *
   */

  Real x_1, x_2, t;
  Real y_1, y_2;
  Real sum = 0;
  for( int i = ai; i < bi-1; i++)
  {
    // x_1 -> X(i)
    // x_2 -> X(i+1)
    // y_1 -> Y(i)
    // y_2 -> Y(i+1)
    x_1 = X[i];
    x_2 = X[i+1];
    y_1 = Y[i];
    y_2 = Y[i+1];
    // X(ai) is to the RIGHT of _a
    // X(bi) is to the RIGHT of _b, but i only goes up to bi-2 and
    // X(bi-1) is to the LEFT of _b
    // therefore, we are just handling interior elements in this loop.
    sum += (x_2 - x_1)*( 0.5*(y_1 + y_2) + (1./12)*(a[i] + b[i]) );
  }


  // now we need to handle the area between [_a,X(ai)] and [X(bi-1),_b]


  // [X(0),_b]
  // x_1 -> X(bi-1)
  // x_2 -> X(bi)
  // y_1 -> Y(bi-1)
  // y_2 -> Y(bi)
  x_1 = X[bi-1];
  x_2 = X[bi];
  y_1 = Y[bi-1];
  y_2 = Y[bi];
  t   = (_b - x_1)/(x_2 - x_1);

  // adding area between x_1 and _b
  sum += (x_2 - x_1) * ( ( t - pow(t,2)/2 )*y_1 + pow(t,2)/2.*y_2 + a[bi-1]*(pow(t,2) - 2.*pow(t,3)/3. + pow(t,4)/4.) + b[bi-1]*(pow(t,3)/3. - pow(t,4)/4.) );

  //
  // [_a,X(0)]
  // x_1 -> X(ai-1)
  // x_2 -> X(ai)
  // y_1 -> Y(ai-1)
  // y_2 -> Y(ai)
  x_1 = X[ai-1];
  x_2 = X[ai];
  y_1 = Y[ai-1];
  y_2 = Y[ai];
  t   = (_a - x_1)/(x_2 - x_1);

  // subtracting area from x_1 to _a
  sum -= (x_2 - x_1) * ( ( t - pow(t,2)/2 )*y_1 + pow(t,2)/2.*y_2 + a[ai-1]*(pow(t,2) - 2.*pow(t,3)/3. + pow(t,4)/4.) + b[ai-1]*(pow(t,3)/3. - pow(t,4)/4.) );

  if( ai != bi ) // _a and _b are not in the in the same element, need to add area of element containing _a
    sum += (x_2 - x_1)*( 0.5*(y_1 + y_2) + (1./12)*(a[ai-1] + b[ai-1]) );

  return sign*sum;

}


/**
 * @brief Calculates the interpolation coefficients from x and y data.
 */
template<typename Real>
void
SplineInterpolator<Real>::calcCoefficients()
{
  const VectorType &X = *(this->xv);
  const VectorType &Y = *(this->yv);

  this->a = VectorType(X.size()-1);
  this->b = VectorType(X.size()-1);

  // we need to solve A x = b, where A is a matrix and b is a vector...
  // isn't that what we are always doing?
  
  /*
   * Solves Ax=B using the Thomas algorithm, because the matrix A will be tridiagonal and diagonally dominant.
   *
   * The method is outlined on the Wikipedia page for Tridiagonal Matrix Algorithm
   */

  //init the matrices that get solved
  VectorType Aa(X.size()), Ab(X.size()), Ac(X.size());
  VectorType b(X.size());


  //Ac is a vector of the upper diagonals of matrix A
  //
  //Since there is no upper diagonal on the last row, the last value must be zero.
  for (size_t i = 0; i < X.size()-1; ++i)
  {
      Ac(i) = 1/(X(i+1) - X(i));
  }

  //This is the line that was causing breakage for n=odd. It was Ac(X.size()) and should have been Ac(X.size()-1)
  Ac(X.size()-1) = 0.0;

  //Ab is a vector of the diagnoals of matrix A
  Ab(0) = 2/(X(1) - X(0));
  for (size_t i = 1; i < X.size()-1; ++i)
  {
      Ab(i) = 2 / (X(i)-X(i-1)) + 2 / (X(i+1) - X(i));
  }
  Ab(X.size()-1) = 2/(X(X.size()-1) - X(X.size()-1-1));


  //Aa is a vector of the lower diagonals of matrix A
  //
  //Since there is no upper diagonal on the first row, the first value must be zero.
  Aa(0) = 0.0;
  for (size_t i = 1; i < X.size(); ++i)
  {
      Aa(i) = 1 / (X(i) - X(i-1));
  }



  // setup RHS vector
  for(int i = 0; i < X.size(); ++i)
  {   
      if(i == 0)
      {   
        b(i) = 3 * ( Y(i+1) - Y(i) )/pow(X(i+1)-X(i),2);
      }
      else if( i == X.size()-1 )
      {   
        b(i) = 3 * (Y(i) - Y(i-1))/pow(X(i) - X(i-1),2);
      }
      else
      { 
        b(i) = 3 * ( (Y(i) - Y(i-1))/(pow(X(i)-X(i-1),2)) + (Y(i+1) - Y(i))/pow(X(i+1) - X(i),2));     
      }
  }


  VectorType c_star(X.size());

  c_star(0) = Ac(0)/Ab(0);
  for (size_t i = 1; i < c_star.size(); ++i)
  {
     c_star(i) = Ac(i) / (Ab(i)-Aa(i)*c_star(i-1)); 
  }

  VectorType d_star(X.size());
  d_star(0) = b(0)/Ab(0);

  for (size_t i = 1; i < X.size(); ++i)
  {
      d_star(i) = (b(i) - Aa(i)*d_star(i-1))/(Ab(i)-Aa(i)*c_star(i-1));
  }

  VectorType x(X.size());
  x( X.size() - 1 ) = d_star( X.size() - 1 );

  for (size_t i = X.size() - 1; i-- > 0;)
  {
      x(i) = d_star(i) - c_star(i)*x(i+1);
  }

  for (int i = 0; i < X.size() - 1; ++i)
  {
      this->a(i) = x(i) * (X(i+1)-X(i)) - (Y(i+1) - Y(i));
      this->b(i) = -x(i+1) * (X(i+1) - X(i)) + (Y(i+1) - Y(i));
  }

}

}

#endif

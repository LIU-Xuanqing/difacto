/**
 * Copyright (c) 2015 by Contributors
 */
#ifndef DIFACTO_COMMON_SPMM_H_
#define DIFACTO_COMMON_SPMM_H_
#include <cstring>
#include <vector>
#include "dmlc/data.h"
#include "dmlc/omp.h"
#include "./range.h"
namespace difacto {

/**
 * \brief multi-thread sparse matrix dense matrix multiplication
 */
class SpMM {
 public:
  /** \brief row major sparse matrix */
  using SpMat = dmlc::RowBlock<unsigned>;
  /**
   * \brief y = D * x
   * @param D n * m sparse matrix
   * @param x m * k length vector
   * @param y n * k length vector, should be pre-allocated
   * @param nthreads optional number of threads
   */
  template<typename V>
  static void Times(const SpMat& D,
                    const std::vector<V>& x,
                    std::vector<V>* y,
                    int nt = DEFAULT_NTHREADS) {
    if (x.empty()) return;
    CHECK_NOTNULL(y);
    int dim = static_cast<int>(y->size() / D.size);
    Times<V>(D, x.data(), y->data(), dim, nt);
  }
  /**
   * \brief y = D^T * x
   * @param D n * m sparse matrix
   * @param x n * k length vector
   * @param y m * k length vector, should be pre-allocated
   * @param nthreads optional number of threads
   */
  template<typename V>
  static void TransTimes(const SpMat& D,
                         const std::vector<V>& x,
                         std::vector<V>* y,
                         int nt = DEFAULT_NTHREADS) {
    TransTimes<V>(D, x, 0, std::vector<V>(), y, nt);
  }
  /**
   * \brief y = D^T * x + p * z
   * @param D n * m sparse matrix
   * @param x n * k length vector
   * @param p scalar
   * @param z m * k length vector,
   * @param y m * k length vector, should be pre-allocated
   * @param nthreads optional number of threads
   */
  template<typename V>
  static void TransTimes(const SpMat& D,
                         const std::vector<V>& x,
                         V p,
                         const std::vector<V>& z,
                         std::vector<V>* y,
                         int nt = DEFAULT_NTHREADS) {
    if (x.empty()) return;
    int dim = x.size() / D.size;
    if (z.size() == y->size() && p != 0) {
      TransTimes<V>(D, x.data(), z.data(), p, y->data(), y->size(), dim, nt);
    } else {
      TransTimes<V>(D, x.data(), NULL, 0, y->data(), y->size(), dim, nt);
    }
  }

 private:
  // y = D * x
  template<typename V>
  static void Times(const SpMat& D, const V* const x,
                    V* y, int dim, int nt = DEFAULT_NTHREADS) {
    memset(y, 0, D.size * dim * sizeof(V));
#pragma omp parallel num_threads(nt)
    {
      Range rg = Range(0, D.size).Segment(
          omp_get_thread_num(), omp_get_num_threads());

      for (size_t i = rg.begin; i < rg.end; ++i) {
        if (D.offset[i] == D.offset[i+1]) continue;
        V* y_i = y + i * dim;
        if (D.value) {
          for (size_t j = D.offset[i]; j < D.offset[i+1]; ++j) {
            V const* x_j = x + D.index[j] * dim;
            V v = D.value[j];
            for (int k = 0; k < dim; ++k) y_i[k] += x_j[k] * v;
          }
        } else {
          for (size_t j = D.offset[i]; j < D.offset[i+1]; ++j) {
            V const* x_j = x + D.index[j] * dim;
            for (int k = 0; k < dim; ++k) y_i[k] += x_j[k];
          }
        }
      }
    }
  }

  // y = D' * x
  template<typename V>
  static void TransTimes(const SpMat& D, const V* const x,
                         const V* const z, V p,
                         V* y, size_t y_size, int dim,
                         int nt = DEFAULT_NTHREADS) {
    if (z) {
      for (size_t i = 0; i < y_size; ++i) y[i] = z[i] * p;
    } else {
      memset(y, 0, y_size*sizeof(V));
    }

#pragma omp parallel num_threads(nt)
    {
      Range rg = Range(0, y_size/dim).Segment(
          omp_get_thread_num(), omp_get_num_threads());

      for (size_t i = 0; i < D.size; ++i) {
        if (D.offset[i] == D.offset[i+1]) continue;
        V const * x_i = x + i * dim;
        if (D.value) {
          for (size_t j = D.offset[i]; j < D.offset[i+1]; ++j) {
            unsigned e = D.index[j];
            if (rg.Has(e)) {
              V v = D.value[j];
              V* y_j = y + e * dim;
              for (int k = 0; k < dim; ++k) y_j[k] += x_i[k] * v;
            }
          }
        } else {
          for (size_t j = D.offset[i]; j < D.offset[i+1]; ++j) {
            unsigned e = D.index[j];
            if (rg.Has(e)) {
              V* y_j = y + e * dim;
              for (int k = 0; k < dim; ++k) y_j[k] += x_i[k];
            }
          }
        }
      }
    }
  }
};

}  // namespace difacto
#endif  // DIFACTO_COMMON_SPMM_H_

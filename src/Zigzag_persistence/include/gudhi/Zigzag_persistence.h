/*    This file is a prototype for the Gudhi Library.
 *    Author(s):       Clément Maria
 *    Copyright (C) 2021 Inria
 *    This version is under developement, please do not redistribute this software. 
 *    This program is for academic research use only. 
 */

#ifndef ZIGZAG_PERSISTENCE_H_
#define ZIGZAG_PERSISTENCE_H_

#include <boost/tuple/tuple.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/pending/disjoint_sets.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/progress.hpp>
#include <cmath>


// #define _VERBATIM_ZIGZAG_PERSISTENCE_ 
// #define _PROFILING_ZIGZAG_PERSISTENCE_
// #define _DEBUG_ZIGZAG_PERSISTENCE_


#ifdef _PROFILING_ZIGZAG_PERSISTENCE_
//#calls to plus_equal_column
static long _num_total_plus_equal_col_                  = 0;
//#calls to arrow_transposition_case_study
static long _num_arrow_trans_case_study_                = 0;
//#calls to plus_equal_column from method arrow_transposition_case_study ~99% of all calls to plus_equal_column
static long _num_arrow_trans_case_study_plus_equal_col_ = 0;
//for every plus_equal_col between col1 and col2, do += |col1| + |col2|
static unsigned long long _total_length_columns_        = 0;
//for every plus_equal_col between col1 and col2, do += |col1|
static unsigned long long _total_length_columns_c1        = 0;
//for every plus_equal_col between col1 and col2, do += |col2|
static unsigned long long _total_length_columns_c2        = 0;
// static unsigned long long _curr_col_is_c1_ = 0;
// static unsigned long long _curr_pcol_is_c1_ = 0;
// static unsigned long long _other_col_is_c1_ = 0;
// static unsigned long long _other_pcol_is_c1_ = 0;
//#iterations of all of the 11 possible subcases of arrow_transposition_case_study. When printed in compute_persistence(), a '+' indicates that curr_col is returned.
static unsigned long long case1 = 0;
static unsigned long long case2 = 0;
static unsigned long long case3 = 0;
static unsigned long long case4 = 0;
static unsigned long long case5 = 0;
static unsigned long long case6 = 0;
static unsigned long long case7 = 0;
static unsigned long long case8 = 0;
static unsigned long long case9 = 0;
static unsigned long long case10 = 0;
static unsigned long long case11 = 0;
//number of arrow transpose per call to backward arrow. map[num arrow transpose] = num of ocurrence of the situation.
static std::map<long, long> _stats_num_arrow_trans_per_backarrow_ = std::map<long,long>();
static double timing_num_arrow_smaller_than_1 = 0.;
static double timing_num_arrow_larger_than_1 = 0.;

//cumulative time taken by the successive arrow transpositions when we do < _X_ or >= _X_ of them
static size_t _X_ = 30;
static double timing_num_arrow_smaller_than_X = 0.;
static double timing_num_arrow_larger_than_X = 0.;
//cumulative time taken by the successive arrow transpositions when we the starting curr_col has < _Y_ entries or >= _Y_ entries.
static size_t _Y_ = 20;
static double timing_entry_length_smaller_than_Y = 0.;
static double timing_entry_length_larger_than_Y = 0.;

static size_t _Z_ = 50;
static double timing_exit_length_smaller_than_Z = 0.;
static double timing_exit_length_larger_than_Z = 0.;

// static unsigned long long num_times_arrow_smaller_than_X = 0;
// static unsigned long long num_times_arrow_larger_than_X = 0;
//sum all of "entry length" of column curr_col when entering a loop of arrow_transposition that lasts less than X iterations. -> to compute the avegra
// static unsigned long long entry_length_arrow_smaller_than_X = 0;
// static unsigned long long entry_length_arrow_larger_than_X = 0;
// static unsigned long long entry_length_arrow_smaller_than_X = 0;
// static unsigned long long entry_length_arrow_larger_than_X = 0;
#endif


namespace Gudhi {

namespace zigzag_persistence {
//represent matrix columns with sets.
struct Zigzag_persistence_colset;
//----------------------------------------------------------------------------------
/** \class Zigzag_persistence Zigzag_persistence.h gudhi/Zigzag_persistence.h
  * \brief Computation of the zigzag persistent homology of a zigzag
  * filtered complex.
  *
  * \details The type ZigzagFilteredComplex::Simplex_key counts the number of 
  * insertions and
  * deletions of simplices, which may be large in zigzag persistence and require
  * more than 32 bits of storage. The type used (int, long, etc) should be chosen in
  * consequence. Simplex_key must be signed.
  *
  * Over all insertions, the Simplex_key must be positive and strictly increasing 
  * when forward iterating along the zigzag filtration.
  */
template < typename ZigzagFilteredComplex
         , typename ZigzagPersistenceOptions = Zigzag_persistence_colset >
class Zigzag_persistence {
public:
  typedef ZigzagFilteredComplex                 Complex;
  typedef ZigzagPersistenceOptions              Options;
/*** Types defined in the complex ***/
  // Data attached to each simplex to interface with a Property Map.
  typedef typename Complex::Simplex_key         Simplex_key;//must be signed
  typedef typename Complex::Simplex_handle      Simplex_handle;
  typedef typename Complex::Filtration_value    Filtration_value;
//
private:
/*** Matrix cells and columns types ***/
  struct matrix_row_tag; // for horizontal traversal in the persistence matrix
  struct matrix_column_tag; // for vertical traversal in the persistence matrix
  typedef boost::intrusive::list_base_hook< 
            boost::intrusive::tag < matrix_row_tag >              //allows .unlink()
          , boost::intrusive::link_mode < boost::intrusive::auto_unlink >
                                                    > base_hook_matrix_row_list;
//hook for a column represented by an intrusive list
  typedef boost::intrusive::list_base_hook <  //faster hook, less safe
            boost::intrusive::tag < matrix_column_tag >
          // , boost::intrusive::link_mode < boost::intrusive::auto_unlink >
            , boost::intrusive::link_mode < boost::intrusive::safe_link >
                                                    > base_hook_matrix_column_list;
//hook for a column represented by an intrusive set
  typedef boost::intrusive::set_base_hook <  //faster hook, less safe
            boost::intrusive::tag < matrix_column_tag >
          , boost::intrusive::optimize_size<true>
          // , boost::intrusive::link_mode < boost::intrusive::auto_unlink >
            , boost::intrusive::link_mode < boost::intrusive::safe_link >  
                                                     > base_hook_matrix_column_set;
//the data structure for columns is selected in Options::searchable_column
  typedef typename std::conditional<Options::searchable_column,
                                    base_hook_matrix_column_set,
                                    base_hook_matrix_column_list >::type 
                                                            base_hook_matrix_column;
//the only option for rows is the intrusive list
  typedef base_hook_matrix_row_list                         base_hook_matrix_row;

  /* Cell for the persistence matrix. Contains a key for the simplex index, and
   * horizontal and vertical hooks for connections within sparse rows and columns.
   */
  struct matrix_chain;//defined below, a chain contains a row, a column, and more
  /** Type of cell in the sparse homology matrix.
   *  For now, only coefficients in Z/2Z, so only the row index (called key) is 
   * stored in the cell.
   */
  struct Zigzag_persistence_cell 
  : public base_hook_matrix_row, public base_hook_matrix_column 
  {
    Zigzag_persistence_cell(Simplex_key key, matrix_chain *self_chain)
    : key_(key)
    , self_chain_(self_chain)
    {}

    Simplex_key key() const { return key_; }
  //compare by increasing key value
    friend bool operator<( const Zigzag_persistence_cell& c1
                         , const Zigzag_persistence_cell& c2) {
      return c1.key() < c2.key();
    }
  /* In a matrix M, if M[i][j] == x not 0, we represent a cell with key_=i 
   * (the row index),
   * self_chain_ points to the chain corresponding the j-th column, and x_=x. 
   * Currently, only Z/2Z coefficients are implemented, so x=1.
   *
   * A cell is connected to all cells of the same row, and all cells of the same 
   * column, via the two boost::intrusive hooks (row and column).
   */
    Simplex_key       key_;
    matrix_chain    * self_chain_;
  };

  //Homology matrix cell
  typedef Zigzag_persistence_cell                                              Cell;
  // Remark: constant_time_size must be false because base_hook_matrix_row and 
  // base_hook_matrix_column have auto_unlink link_mode
  //vertical list of cells, forming a matrix column stored as an intrusive list
  typedef boost::intrusive::list < 
                  Cell
                , boost::intrusive::constant_time_size<false>
                , boost::intrusive::base_hook< base_hook_matrix_column_list >  > 
                                                                        Column_list;
  //vertical list of cells, forming a matrix column stored as an intrusive set
  typedef boost::intrusive::set < 
                  Cell
                , boost::intrusive::constant_time_size<false>
                , boost::intrusive::base_hook< base_hook_matrix_column_set >  >
                                                                         Column_set;
  //choice encoded in Options::searchable_column. a column can be
  //iterated through, and keys are read in strictly increasing natural order. 
  typedef typename std::conditional<Options::searchable_column,
                                    Column_set,
                                    Column_list >::type                      Column;
  //horizontal list of cells, forming a matrix row, no particular order on keys.
  typedef boost::intrusive::list < 
                  Cell
                , boost::intrusive::constant_time_size<false>
                , boost::intrusive::base_hook< base_hook_matrix_row >  >   
                                                                           Row_list;
  //rows are encoded by lists. need to be sorted and traversed
  typedef Row_list                                                              Row;

  /* Chain for zigzag persistence. A chain stores:
   * - a matrix column (col_i) that represents the chain as a sum of simplices 
   * (represented by their unique key, stored in the cells of the sparse column),
   * - a matrix row of all elements of index the lowest index of the column (row_i),
   * - is paired with another chain, indicating its type F, G, or H,
   * - has a direct access to its lowest index.
   */
  struct matrix_chain {
    /* Trivial constructor, birth == -3 */
    matrix_chain() : column_(nullptr), row_(nullptr), paired_col_(nullptr), 
      birth_(-3), lowest_idx_(-1) {}

    /* Creates a matrix chain of type F with one cell of index 'key'. */
    matrix_chain(Simplex_key key) 
    : paired_col_(nullptr), birth_(key), lowest_idx_(key) 
    {
      Cell *new_cell = new Cell(key, this);
      if constexpr(Options::searchable_column) { column_.insert(*new_cell); }
      else                                     { column_.push_back(*new_cell); }
      row_.push_back(*new_cell);
    }
    /* Creates a matrix chain of type F with new cells of key indices given by a 
     * range. Birth and lowest indices are given by 'key'. 
     * The range [beg,end) must be sorted by increasing key values, the same 
     * order as the column_ when read from column_.begin() to column_.end().
     *
     * SimplexKeyIterator value_type must be Simplex_key. 
     * KeyToMatrixChain must be of type 
     *         std::map< Simplex_key, typename std::list<matrix_chain>::iterator >
     */
    template< typename SimplexKeyIterator, typename KeyToMatrixChain >
    matrix_chain(Simplex_key key, SimplexKeyIterator beg, SimplexKeyIterator end, KeyToMatrixChain &lowidx_to_matidx) 
    : paired_col_(nullptr), birth_(key), lowest_idx_(key) 
    {
      for(SimplexKeyIterator it = beg; it != end; ++it) 
      {
        Cell *new_cell = new Cell(*it, this);//create a new cell
        //insertion in the column
        if constexpr(Options::searchable_column) { 
          column_.insert(column_.end(), *new_cell); //ordered range
        }
        else { column_.push_back(*new_cell); }
        //insertion in a row, not corresponding to the row stored in this->row_.
        lowidx_to_matidx[*it]->row_.push_back( *new_cell ); 
      }
      //Add the bottom coefficient for the chain
      Cell *new_cell = new Cell(key, this);
      //insertion in the column, key is larger than any *it in [beg, end) above.
      if constexpr(Options::searchable_column) { 
        column_.insert(column_.end(), *new_cell); 
      }
      else { column_.push_back(*new_cell); }
      //insertion of row_, that stores no particular order.
      row_.push_back( *new_cell );
    }

   /* Creates a matrix chain of type H with new cells of key indices given by a 
    * range. Birth and lowest indices are given by 'key'. 
    * The range [beg,end) must be sorted by increasing key values, the same 
    * order as the column_ when read from column_.begin() to column_.end().
    *
    * SimplexKeyIterator value_type must be Simplex_key. 
    * KeyToMatrixChain must be of type 
    *             std::map< Simplex_key, typename std::list<matrix_chain>::iterator >
    */
    template< typename SimplexKeyIterator, typename KeyToMatrixChain >
    matrix_chain(Simplex_key key, matrix_chain *paired_col, SimplexKeyIterator beg, SimplexKeyIterator end, KeyToMatrixChain &lowidx_to_matidx) 
    : paired_col_(paired_col), birth_(-2), lowest_idx_(key) 
    {
      for(SimplexKeyIterator it = beg; it != end; ++it) 
      {
        Cell * new_cell = new Cell(*it, this);//create a new cell
        //insertion in the column
        if constexpr(Options::searchable_column) { 
          column_.insert(column_.end(), *new_cell); //ordered range
        }
        else { column_.push_back(*new_cell); }
        //insertion in a row, not corresponding to the row stored in this->row_.
        lowidx_to_matidx[*it]->row_.push_back( *new_cell ); 
      }
      //Add the bottom coefficient for the chain
      Cell * new_cell = new Cell(key, this);
      //insertion in the column, key is larger than any *it in [beg, end) above.
      if constexpr(Options::searchable_column) { 
        column_.insert(column_.end(), *new_cell); 
      }
      else { column_.push_back(*new_cell); }
      //insertion of row_, that stores no particular order.
      row_.push_back( *new_cell );
    }

    /* Erase the chain, all cells were allocated with operator new. */
    ~matrix_chain() 
    { //empty the column, call delete on all cells
      for(typename Column::iterator c_it = column_.begin(); c_it != column_.end(); )
      {
        auto tmp_it = c_it; ++c_it;
        Cell * tmp_cell = &(*tmp_it);
        tmp_it->base_hook_matrix_row::unlink(); //rm from row
        column_.erase(tmp_it);
        delete tmp_cell;
      }
    }

   /* Returns the chain with which *this is paired in the F,G,H classification. 
    * If in F (i.e., paired with no other column), return nullptr.*/
    matrix_chain * paired_chain() { return paired_col_; }
   /* Assign a paired chain. */
    void assign_paired_chain(matrix_chain *other_col) { paired_col_ = other_col; }
    /* Access the column. */
    Column & column() { return column_; }
    /* Returns the birth index (b >= 0) of the chain if the column is in F. 
     * Returns -2 if the chain is in H, and -1 if the chain is in G. */
    Simplex_key birth()                       { return birth_; }
    /* Assign a birth index to the chain. */
    void assign_birth(Simplex_key b)          { birth_ = b; }
    void assign_birth(matrix_chain *other) { birth_ = other->birth_; }
    /* Returns true iff the chain is indexed in F. */
    bool inF() { return birth_ >  -1; }
    /* Returns true iff the chain is indexed in G. */
    bool inG() { return birth_ == -1; }
    /* Returns true iff the chain is indexed in H. */
    bool inH() { return birth_ == -2; }

    Column            column_      ; //col at index i, with lowest index i
    Row               row_         ; //row at index i
    matrix_chain    * paired_col_  ; //\in F -> nullptr, \in H -> g, \in G -> h
    Simplex_key       birth_       ; //\in F -> b, \in H -> -2 \in G -> -1
    Simplex_key       lowest_idx_  ; //lowest_idx_ = i (upper triangular matrix)
  };

  public:
/* Structure to store persistence intervals. By convention, interval [b;d] are
 * closed for finite indices b and d, and open for left-infinite and/or
 * right-infinite endpoints.*/
  struct interval_t {
    interval_t() {}
    interval_t(int dim, double b, double d) : dim_(dim), b_(b), d_(d) {}
  /* Returns the absolute length. */
    Filtration_value length() {
      if(b_ == d_) { return 0; } //otherwise inf - inf would return nan.
      return abs(b_ - d_);
    }
  /* Returns the differences of log values. */
    Filtration_value log_length() {//return the log-length
      if(b_ == d_) { return 0; } //otherwise inf - inf would return nan.
      return abs(log2((double)b_) - log2((double)d_));
    }
  /* Returns the homological dimension corresponding to the interval. */
    int dim() { return dim_; }//return the homological dimension of the interval
    Filtration_value birth() { return b_; }//return the birth value
    Filtration_value death() { return d_; }//return the death value

  // private://note that we don't assume b_ <= d_  
    int              dim_; //homological dimension
    Filtration_value b_; //filtration value associated to birth index
    Filtration_value d_; //filtration value associated to death index
  };

private:
  /* Comparison function to sort intervals by decreasing log-length in the 
   * output persistence diagram, i.e.,
   * [f(b),f(d)]<[f(b'),f(d')] iff |log2(f(b))-log2(f(d))|> |log2(f(b'))-log2(f(d'))|
   */
  struct cmp_intervals_by_log_length {
    cmp_intervals_by_log_length(){}
    bool operator()( interval_t p, interval_t q)
    { 
      if(p.dim() != q.dim()) {return p.dim() < q.dim();}//lower dimension first
      if(p.log_length() != q.log_length()) {return p.log_length() > q.log_length();}
      if(p.birth() != q.birth()) {return p.birth() < q.birth();}//lex order
      return p.death() < q.death();
    }
  };
  /* Comparison function to sort intervals by decreasing length in the 
   * output persistence diagram, i.e.,
   * [f(b),f(d)]<[f(b'),f(d')] iff  |f(b)-f(d)| > |f(b')-f(d')|
   */
  struct cmp_intervals_by_length {
    cmp_intervals_by_length(){}
    bool operator()( interval_t p, interval_t q)
    {
      if(p.length() != q.length()) { return p.length() > q.length(); }//longest 1st
      if(p.dim() != q.dim()) {return p.dim() < q.dim();}//lower dimension first
      if(p.birth() != q.birth()) {return p.birth() < q.birth();}//lex order
      return p.death() < q.death();
    }
  };

public:
  Zigzag_persistence( Complex &cpx )
  : cpx_(&cpx)
  , dim_max_(-1)
  , lowidx_to_matidx_()
  , matrix_()
  , birth_ordering_()
  , persistence_diagram_()
  , num_arrow_(0)
  , filtration_values_() {}

private:
/* Set c1 <- c1 + c2, assuming canonical order of indices induced by the order in
 * the vertical lists. self1 is the matrix_chain whose column is c1, for self
 * reference of the new cells.
 */
  void plus_equal_column(matrix_chain * self1, Column & c1, Column & c2)
  {

#ifdef _PROFILING_ZIGZAG_PERSISTENCE_
    ++_num_total_plus_equal_col_;
    _total_length_columns_ += (c1.size() + c2.size());
    _total_length_columns_c1 += c1.size();
    _total_length_columns_c2 += c2.size();
#endif

    //insert all elements of c2 in c1, in O(|c2| * log(|c1|+|c2|))
    if constexpr (Options::searchable_column) {
      for(auto &cell : c2) {
        auto it1 = c1.find(cell);
        if(it1 != c1.end()) {//already there => remove as 1+1=0
          Cell * tmp_ptr = &(*it1);
          it1->base_hook_matrix_row::unlink(); //unlink from row
          c1.erase(it1); //remove from col
          delete tmp_ptr;
        }
        else {//not there, insert new cell
          Cell *new_cell = new Cell(cell.key(), self1);
          c1.insert(*new_cell);
          lowidx_to_matidx_[cell.key()]->row_.push_back(*new_cell);//row link,no order
        }
      }
    }
    else {//traverse both columns doing a standard column addition, in O(|c1|+|c2|)
      auto it1 = c1.begin();   auto it2 = c2.begin();
      while(it1 != c1.end() && it2 != c2.end())
      {
        if(it1->key() < it2->key()) { ++it1; }
        else {
          if(it1->key() > it2->key()) {
            Cell * new_cell = new Cell(it2->key(), self1);
            c1.insert(it1, *new_cell); //col link, in order
            lowidx_to_matidx_[it2->key()]->row_.push_back(*new_cell);//row link,no order
            ++it2;
          }
          else { //it1->key() == it2->key()
            auto tmp_it = it1;    ++it1; ++it2;
            Cell * tmp_ptr = &(*tmp_it);
            tmp_it->base_hook_matrix_row::unlink(); //unlink from row
            c1.erase(tmp_it); //remove from col
            delete tmp_ptr;
          }
        }
      }
      while(it2 != c2.end()) {//if it1 reached the end of its column, but not it2
        Cell * new_cell = new Cell(it2->key(),self1);
        lowidx_to_matidx_[it2->key()]->row_.push_back(*new_cell); //row links
        c1.push_back(*new_cell);
        ++it2;
      }
    }
  }

/** Maintains the birth ordering <=b. Contains an std::map of size the number of
  * non-zero rows of the homology matrix, at any time during the computation of
  * zigzag persistence.
  *
  * By construction, we maintain the map satisfying 
  * 'birth_to_pos_[i] < birth_to_pos_[j]', 
  * with 0 <= i,j <= k indices in the quiver '0 \leftrightarrow ... \leftrightarrow i \leftrightarrow .. \leftrightarrow k' 
  * visited at time k of the algorithm (prefix of length k of the full zigzag 
  * filtration '0 \leftrightarrow ... \leftrightarrow i \leftrightarrow .. \leftrightarrow k \leftrightarrow ... \leftrightarrow n' that is studied), 
  * iff i <b j for the birth ordering. 
  *
  * By construction, when adding index k+1 to '0 \leftrightarrow ... \leftrightarrow i \leftrightarrow .. \leftrightarrow k \leftrightarrow k+1',
  * we have:
  * - if k -> k+1 forward, then j <b k+1 for all indices j < k+1, otherwise
  * - if k <- k+1 backward, then k+1 <b j for all indices j < k+1.
  */
  struct birth_ordering {
    //example quiver indices    empty_cpx -> 0 -> 1 -> 2 <- 3 <- 4 -> 5 <- 6 etc
    birth_ordering() : birth_to_pos_(), max_birth_pos_(0), min_birth_pos_(-1) {}

    //when the arrow key-1 -> key is forward, key is larger than any other index
    //i < key in the birth ordering <b. We give key the largest value max_birth_pos_
    void add_birth_forward(Simplex_key key) { //amortized constant time
      birth_to_pos_.emplace_hint(birth_to_pos_.end(), key, max_birth_pos_);
      ++max_birth_pos_;
    }
    //when the arrow key-1 <- key is backward, key is smaller than any other index
    //i < key in the birth ordering <b. We give key the smallest value min_birth_pos_
    void add_birth_backward(Simplex_key key) { //amortized constant time
      birth_to_pos_.emplace_hint(birth_to_pos_.end(), key, min_birth_pos_);
      --min_birth_pos_;
    }
    //when the row at index key is removed from the homology matrix, we do not need
    //to maintain its position in <b anymore
    void remove_birth(Simplex_key key) { birth_to_pos_.erase(key); }
    //increasing birth order <=b, true iff k1 <b k2
    bool birth_order(Simplex_key k1, Simplex_key k2) {
        return birth_to_pos_[k1] < birth_to_pos_[k2];
    }
    //decreasing birth order <=b, true iff k1 >b k2
    bool reverse_birth_order(Simplex_key k1, Simplex_key k2) {
        return birth_to_pos_[k1] > birth_to_pos_[k2];
    }

  private:
    //birth_to_pos_[i] < birth_to_pos_[j] iff i <b j
    std::map< Simplex_key, Simplex_key > birth_to_pos_;
    //by construction, max_birth_pos_ (resp. min_birth_pos_) is strictly larger 
    //(resp. strictly smaller) than any value assigned to a key so far.
    Simplex_key                          max_birth_pos_;
    Simplex_key                          min_birth_pos_;
  };

public:
/** \brief Computes the zigzag persistent homology of a zigzag filtered complex,
  * using the reflection and transposition algorithm of \cite zigzag_reflection.
  *
  * \details matrix_, originally empty, maintains the set of chains, with a 
  * partition \f$ F \sqcup G \sqcup H\f$
  * representing a compatible homology basis as in \cite zigzag_reflection.
  *
  * Each simplex in the complex stores a .key_ field that stores the index of
  * its insertion in the zigzag filtration.
  *
  * The algorithm maintains a compatible homology basis for the zigzag filtration
  *
  * \f$$\emptyset = K_0 \leftrightarrow (...) \leftrightarrow K_i \leftarrow ... \leftarrow \emptyset\f$$
  * 
  * where the prefix from \f$K_0\f$ to \f$K_i\f$ is equal to the i-th prefix of 
  * the input zigzag
  * filtration given by cpx_->filtration_simplex_range(), and the suffix 
  * (from \f$K_i\f$ 
  * to the right end) is a sequence of simplex removals. Due to the structure of 
  * reflection diamonds, the removal are in reverse order of the insertions, to 
  * reduce the amount of transposition diamonds.
  *
  * Consequently, using cpx_->key(zzsh) as indexing for the matrix rows/cells,
  * with the natural order on integers, makes our homology matrix matrix_ upper
  * triangular for the suffix \f$K_i \leftarrow ... \leftarrow 0\f$, seen as a 
  * standard persistence
  * filtration. At \f$K_i\f$, the natural order on integers is also equivalent to the
  * death-order \f$\leq_d\f$ (because all arrows in the suffix are backward).
  *
  * Insertion: cpx_->key(*zzit) is a strictly increasing sequence for zzit
  * insertion of cells (does not need to be contiguous). However, for every forward
  * arrow, we have cpx_->key(*zzit) == num_arrows_.
  * Removal: cpx_->key(*zzit) gives the assigned key (during past insertion) of a
  * cell == *zzit during a removal. We use num_arrows_ to record the deaths in the
  * persistence diagram.
  * Insertion and Removal: zzit.filtration() is totally monotone. Note that the
  * iterator encodes the filtration, and not the cells within the complex structure.
  */
  void zigzag_persistent_homology()
  { //compute index persistence, interval are closed, i.e., [b,d) is stored as 
    //[b,d-1]. The filtration values are maintained in field filtration_values_
    Filtration_value prev_fil_, curr_fil_;

    assert(num_arrow_ == 0);
    auto zzrg = cpx_->filtration_simplex_range();
    auto zzit = zzrg.begin();
    dim_max_ = zzit.dim_max();

    num_arrow_ = cpx_->key(*zzit);

    prev_fil_ = zzit.filtration(); 
    filtration_values_.emplace_back(num_arrow_, prev_fil_);

    while( zzit != zzrg.end() )
    { 

#ifdef _PROFILING_ZIGZAG_PERSISTENCE_
if( (num_arrow_ % 10000) == 0) {
  std::cout << num_arrow_ << "  av. length col  = " << (double) _total_length_columns_/(double)_num_total_plus_equal_col_ << "\n";
  std::cout << num_arrow_ << "  av. length col1 = " << (double) _total_length_columns_c1/(double)_num_total_plus_equal_col_ << "\n";
  std::cout << num_arrow_ << "  av. length col2 = " << (double) _total_length_columns_c2/(double)_num_total_plus_equal_col_ << "\n";

  std::cout << "  " << _num_arrow_trans_case_study_ << ": " << case1 << " " << case2 << " " << case3 << " " << case4 << " " << case5 << " " << case6 << " " << case7 << " " << case8 << " " << case9 << " " << case10 << " " << case11 << " \n" ;

  std::cout << "              ->  " << "8:" << 100*(float)case8/(float)_num_arrow_trans_case_study_ << " +    F x H\n";
  std::cout << "              ->  " << "10:" << 100*(float)case10/(float)_num_arrow_trans_case_study_ << " +    F x F  &  bs <b bt\n";
  std::cout << "              ->  " << "11:" << 100*(float)case11/(float)_num_arrow_trans_case_study_ << " -    F x F  &  bt <b bs\n" ;
  std::cout << "              ->  1:" << 100*(float)case1/(float)_num_arrow_trans_case_study_ << " +    H x H  &  dgs <d dgt\n";
  std::cout << "              ->  " << "2:" << 100*(float)case2/(float)_num_arrow_trans_case_study_ << " -    H x H  &  dgt <d dgs\n";
  std::cout << "              ->  " << "3:" << 100*(float)case3/(float)_num_arrow_trans_case_study_ << " -    H x F\n";

  std::cout << "              -> total case 8+1+10= " << 100*(float)(case8+case1+case10)/(float)_num_arrow_trans_case_study_ << " % of all\n";

  std::cout << "  #arrow transpose per back arrow: ";
  unsigned long long total_num_arrow_transpose = 0;
  for(auto pp : _stats_num_arrow_trans_per_backarrow_) {
    total_num_arrow_transpose += pp.second;
  }
  std::cout << std::endl;
  std::cout << "  total num arrow transpose = " << total_num_arrow_transpose << "  -  ";
  std::cout << "with 99.9% of back arrow having at most: ";
  unsigned long long cumul = 0;
  long macnum = 0;
  auto it = _stats_num_arrow_trans_per_backarrow_.begin();
  while( (float)cumul/(float)total_num_arrow_transpose <= 0.999 ) {
    cumul += it->second;
    macnum = it->first;
  }
  std::cout << macnum << " arrow transpose.\n";
  std::cout << "cumul time for 0 arrow transpose  vs  for >= 1 arrow transpose: " << timing_num_arrow_smaller_than_1 << "   vs   " << timing_num_arrow_larger_than_1 << "  ratio == " << timing_num_arrow_larger_than_1 / timing_num_arrow_smaller_than_1 << "\n";

  std::cout << "cumul time for < " << _X_ << " arrow transpose  vs  for >=" << _X_ << " arrow transpose: " << timing_num_arrow_smaller_than_X << "   vs   " << timing_num_arrow_larger_than_X << "  ratio many/few == " << timing_num_arrow_larger_than_X / timing_num_arrow_smaller_than_X << "\n";

  std::cout << "cumul time for entry col < " << _Y_ << " entries  vs  >=" << _Y_ << " entries: " << timing_entry_length_smaller_than_Y << "   vs   " << timing_entry_length_larger_than_Y << "  ratio longer/shorter == " << timing_entry_length_larger_than_Y/timing_entry_length_smaller_than_Y << "\n";
  std::cout << "cumul time for exit col < " << _Z_ << " entries  vs  >=" << _Z_ << " entries: " << timing_exit_length_smaller_than_Z << "   vs   " << timing_exit_length_larger_than_Z << "  ratio longer/shorter == " << timing_exit_length_larger_than_Z/timing_exit_length_smaller_than_Z << "\n";

  stat_mat();

}
#endif
#ifdef _VERBATIM_ZIGZAG_PERSISTENCE_
//print the simplex
  std::cout  << " #" << num_arrow_ << "     ";      
  std::cout << "[" << zzit.filtration() << "] ";
  if(zzit.arrow_direction()) { std::cout << "-> "; }
  else { std::cout << "<- "; }
  for(auto v : cpx_->simplex_vertex_range(*zzit)) { std::cout << v << " "; }
  std::cout << std::endl;
#endif


      //insertion of a simplex
      if(zzit.arrow_direction()) { num_arrow_ = cpx_->key(*zzit); } 
      else { ++num_arrow_; } //removal of a simplex, a simplex key corresponds to the index of its INSERTION

      curr_fil_ = zzit.filtration();//cpx_->filtration(*zzit) is invalid for (<-);
      if(curr_fil_ != prev_fil_) //check whether the filt value has changed
      { //consecutive pairs (i,f), (j,f') mean simplices of index k in [i,j-1] have
        prev_fil_ = curr_fil_;                                 //filtration value f
        filtration_values_.emplace_back(num_arrow_, prev_fil_);
      }
      //Iterator zzit gives all cells for insertion (forward arrows). We can ignore
      //cells that are not critical. 
      //The iterator gives both critical and non-critical cells for deletion 
      //(backward arrows).
      
    // #
    // #
    // #
    // #
     //  ...how to we know if we remove a free pair or just one elemnt of a free pair?
     //  #
     //  #
     //  #
     //  #
      //The later case happens when the deletion of a cell sigma in a Morse pair
      //(tau,sigma) happens with a different filtration value than the one of the
      //deletion of tau. At the moment of deletion, sigma is a maximal cell, 
      //all keys
      //have been assigned correctly by the filtration_simplex_iterator, both tau 
      //and
      //sigma must be elevated into critical cells. This last operation is
      //implemented in make_pair_critical. sigma is then removed like a normal cell.
      if(zzit.arrow_direction()) { //forward arrow, only consider critical cells
        // if(cpx_->critical(*zzit)) { 
          forward_arrow(*zzit); 
      }
      else { //backward arrow
        //matrix A becomes matrix A U \{\tau,sigma\}
        // if(!cpx_->critical(*zzit)) { std::cout << "ERROR\n"; make_pair_critical(*zzit); }

        backward_arrow(*zzit);
      }
      ++zzit;
    }

    if(!matrix_.empty()) { 
      std::cout << "There remain " << matrix_.size() << " columns in the matrix.\n";
    }


#ifdef _PROFILING_ZIGZAG_PERSISTENCE_
    std::cout << " ------- with:\n";
    std::cout << "   calls to arrow transposition case study:    " << _num_arrow_trans_case_study_ << "\n";
    std::cout << "     with in average arrow_trans/backward_arrow: " << (float)_num_arrow_trans_case_study_/(float)_num_backward_arrow_ << "\n";
    std::cout << "   calls to plus_equal_col in total:           " << _num_total_plus_equal_col_ << "\n";
    std::cout << "   calls to plus_equal_col within arrow_trans: " << _num_arrow_trans_case_study_plus_equal_col_ << "\n";
    std::cout << "     which represents (in %):                  " << (float)_num_arrow_trans_case_study_plus_equal_col_/(float)_num_total_plus_equal_col_ * 100. << "\n";
    std::cout << "\n";
    std::cout << "   average length manipulated column:          " << (double) _total_length_columns_/(double)_num_total_plus_equal_col_ << "\n";
#endif
  
  }

/* sh is a maximal simplex paired with a simplex tsh
 * Morse pair (tau,sigma)
 *
 * sh must be paired with tsh, i.e., tsh = cpx_->morse_pair(zzsh); return the
 * handle for tau, and cpx_->critical(zzsh) is false. The Morse iterator is in
 * charge of modifying this (make the simplices critical) after the
 * Zigzag_persistence update has been done.
 *
 *
 * key(tsh) < key(sh) must be true.
 *
 * cpx_->boundary_simplex_range(zzsh) must iterate along the boundary of sigma in
 * the Morse complex A' where sigma and tau have become critical
 *
 * cpx_->coboundary_simplex_range(tsh) must iterate along the cofaces of tsh in the
 * Morse complex A' where sigma and tau have become critical, i.e., those critical
 * faces \nu such that [\nu : \tau]^{A'} != 0.
 *
 */
  void make_pair_critical(Simplex_handle zzsh)
  {
    // auto tsh = cpx_->morse_pair(zzsh);//Morse pair (*tsh, *sh)
    // //add the filtration value for recording, fil(tsh) == fil(zzsh)
    // //filtration_values_.emplace_back(cpx_->key(tsh), cpx_->filtration(tsh));
    // //new column and row for sigma
    // Column * new_col_s = new Column();
    // Row  * new_row_s   = new Row();
    // matrix_.emplace_front( new_col_s
    //           , new_row_s
    //           , (matrix_chain *)0
    //           , -2 //in H, paired w/ the new column for tau
    //           // , curr_fil_
    //           , cpx_->key(zzsh) );
    // auto chain_it_s = matrix_.begin();
    // //Add the bottom coefficient for zzsh
    // Cell * new_cell_s = new Cell(cpx_->key(zzsh), &(*chain_it_s));
    // new_col_s->push_back( *new_cell_s ); //zzsh has largest idx of all
    // new_row_s->push_back( *new_cell_s );
    // //Update the map 'index idx -> chain with lowest index idx' in matrix_
    // lowidx_to_matidx_[cpx_->key(zzsh)] = chain_it_s;

    // //new column and row for tau
    // Column * new_col_t = new Column();
    // Row *  new_row_t   = new Row();
    // matrix_.emplace_front( new_col_t
    //           , new_row_t
    //           , (matrix_chain *)0
    //           , -1 //in G, paired w/ the new column for sigma
    //           // , curr_fil_
    //           , cpx_->key(tsh) );
    // auto chain_it_t = matrix_.begin();
    // //Update the map 'index idx -> chain with lowest index idx' in matrix_
    // lowidx_to_matidx_[cpx_->key(tsh)] = chain_it_t;

    // //pair the two new columns
    // chain_it_t->assign_paired_chain(&(*chain_it_s));
    // chain_it_s->assign_paired_chain(&(*chain_it_t));

    // if(chain_it_s->lowest_idx_ != cpx_->key(zzsh)) {std::cout << "Error low key\n";}

    // //fill up col_tau with \partial \sigma in new Morse complex
    // std::set< Simplex_key > col_bsh; //set maintains the order on indices
    // for( auto b_sh : cpx_->boundary_simplex_range(zzsh) )//<-\partial in Morse complex
    //     // for( auto b_sh : cpx_->paired_simplex_boundary_simplex_range(zzsh) )//<-\partial in Morse complex
    // { col_bsh.insert(cpx_->key(b_sh)); }
    // //copy \partial sigma in the new row&column
    // for( auto idx : col_bsh ) //in increasing idx order
    // { //add all indices in col_tau with canonical order enforced by set col_bsh
    //   Cell * new_cell = new Cell(idx, &(*chain_it_t));
    //   new_col_t->push_back( *new_cell ); //insertion in column
    //   lowidx_to_matidx_[idx]->row_->push_back( *new_cell ); //insertion in row
    // }

    // //update the row for sigma. First record all possible modified columns
    // std::map<matrix_chain *, int> modif_chain;
    // for(auto c_sh : cpx_->coboundary_simplex_range(tsh)) {//[*c_sh:*t_sh]^{A'} != 0
    //     // for(auto c_sh : cpx_->paired_simplex_coboundary_simplex_range(tsh)) {//[*c_sh:*t_sh]^{A'} != 0
    //     //all chains with != 0 index at c_sh
    //     if(cpx_->key(c_sh) != cpx_->key(zzsh)) {//don't take into account col_sigma
    //      for(auto cell : *(lowidx_to_matidx_[cpx_->key(c_sh)]->row_)) {
   //         auto res_insert = modif_chain.emplace(cell.self_chain_,1);
   //         if(!res_insert.second) {//already there
   //           ++(res_insert.first->second); //one more occurrence of the chain
    //        }
    //      }
    //     }
    // }
    // //all chains appearing an odd number of times
    // for(auto modif : modif_chain) {
    //   if((modif.second % 2) == 1) { //sum_{nu \in chain} [nu:tau]^{A'} = 1
    //  //Add the bottom coefficient for zzsh to rectify new boundary
    //  Cell * new_cell = new Cell(cpx_->key(zzsh), modif.first);
    //  // modif.first->column()->push_back( *new_cell ); //zzsh doesn't have largest idx of all
    //  insert_cell(modif.first->column(), new_cell);
    //  new_row_s->push_back( *new_cell );//row for sigma
    //  // //if chain in H, modify the paired c_g
    //  // if(modif.first->birth() == -2) {
    //  //   auto chain_g = modif.first->paired_col_; //c_g to modify <- add c_tau
    //  //   plus_equal_column(chain_g, chain_g->column(), new_col_t);
    //  // }
    //   }//else sum == 0
    // }
  }

    // Filtration_value index_to_filtration(Simplex_key k) {
    //   auto it =
    //   std::upper_bound( filtration_values_.begin(), filtration_values_.end()
    //          , std::pair<Simplex_key, Filtration_value>(k, std::numeric_limits<double>::infinity() )
    //          , []( std::pair<Simplex_key, Filtration_value> p1
    //              , std::pair<Simplex_key, Filtration_value> p2) {
    //             return p1.first < p2.first; }
    //          );
    //   if(it->first != k) {return (--it)->second;}
    //   else {return it->second;}
    // }

    //Dionysus: In interval
    // (idx i)_R_{eta eps_i}(Pi) -> .. <- R_{eta eps_i+1}(Pi+1)_(idx j), anything
    // created from index i+1 on has birth eps_i+1, anything destroyed has death eps_i
    // Filtration_value index_to_filtration_birth(Simplex_key k) {
    //   auto it = //strictly greater
    //   std::lower_bound( filtration_values_.begin(), filtration_values_.end()
    //          , std::pair<Simplex_key, Filtration_value>(k, std::numeric_limits<double>::infinity() )
    //          , []( std::pair<Simplex_key, Filtration_value> p1
    //              , std::pair<Simplex_key, Filtration_value> p2) {
    //             return p1.first < p2.first; }
    //          );
    //   return it->second;
    // }
    // Filtration_value index_to_filtration_death(Simplex_key k) {
    //   auto it =
    //   std::upper_bound( filtration_values_.begin(), filtration_values_.end()
    //          , std::pair<Simplex_key, Filtration_value>(k, std::numeric_limits<double>::infinity() )
    //          , []( std::pair<Simplex_key, Filtration_value> p1
    //              , std::pair<Simplex_key, Filtration_value> p2) {
    //             return p1.first < p2.first; }
    //          );
    //   --it;
    //   return it->second;
    // }


private: 
/** \brief Computes the boundary cycle of the new simplex zzsh, and express it as a
  * sum of cycles. If all cycles are boundary cycles, i.e., columns with G-index
  * in the matrix, then [\partial zzsh] = 0 and we apply an injective diamond to
  * the zigzag module. Otherwise, we keep reducing with boundary- and live- cycles,
  * i.e., columns with (F \cup G)-indices, and then apply a surjective diamond to
  * the zigzag module. 
  */
  void forward_arrow( Simplex_handle zzsh )
  { //maintain the <=b order
    birth_ordering_.add_birth_forward(cpx_->key(zzsh));

    //Reduce the boundary of zzsh in the basis of cycles.
    //Compute the simplex keys of the simplices of the boundary of zzsh.
    std::set< Simplex_key > col_bsh; //set maintains the natural order on indices
    for( auto b_sh : cpx_->boundary_simplex_range(zzsh) )
    { col_bsh.insert(cpx_->key(b_sh)); }

    //If empty boundary (e.g., when zzsh is a vertex in a simplicial complex)
    //Add a non-trivial cycle [c = zzsh] to the matrix, make it a creator in F.  
    if(col_bsh.empty()) // -> creator
    { //New row and column with a bottom-right non-zero element, at index key(zzsh)
      //i.e., create a new cycle in F, equal to    *zzsh alone.
      matrix_.emplace_front( cpx_->key(zzsh) );
      auto new_chain_it = matrix_.begin();//the new chain
      //Update the map [index idx -> chain with lowest index idx] in matrix_
      lowidx_to_matidx_[cpx_->key(zzsh)] = new_chain_it;
      return;
    }

    // col_bsh.rbegin()) is idx of lowest element in col_bsh, because it is a set.
    matrix_chain *col_low = &(*lowidx_to_matidx_[*(col_bsh.rbegin())]);
    auto paired_idx = col_low->paired_col_; //col with which col_low is paired
    std::vector< matrix_chain * > chains_in_H; //for corresponding indices in H
    std::vector< matrix_chain * > chains_in_G;

    //Reduce col_bsh with boundary cycles, i.e., indices in G.
    std::pair< typename std::set< Simplex_key >::iterator, bool > res_insert;
    while( paired_idx != nullptr ) 
    {
      chains_in_H.push_back(paired_idx);//keep the col_h with which col_g is paired
      chains_in_G.push_back(col_low);   //keep the col_g
      for(auto &cell : (col_low->column())) { //Reduce with the column col_g
        res_insert = col_bsh.insert(cell.key());
        if( !res_insert.second ) { col_bsh.erase(res_insert.first); } //1+1 = 0
        //o.w. insertion has succeeded.
      }
      //If col_bsh is entirely reduced, \partial zzsh is a boundary cycle.
      if(col_bsh.empty()) {
        // if(cpx_->dimension(zzsh) >= max_dim_) {return;} we need max_dim creators
        injective_reflection_diamond(zzsh, chains_in_H);
        return;
      }
      //Continue the reduction
      col_low     =  &(*lowidx_to_matidx_[*(col_bsh.rbegin())]);//curr low index col
      paired_idx  =  col_low->paired_col_;//col with which col_low is paired
    }

    //Continue reducing with boundary and 'live' cycles, i.e., indices in G U F.
    std::vector< matrix_chain * > chains_in_F;
    while(true)
    {
      if(paired_idx == nullptr) { chains_in_F.push_back(col_low); }//col_low is in F
      else { chains_in_H.push_back(paired_idx); } //col_low in G, paired_idx is in H
      //Reduce with the column col_g or col_f
      for(auto &cell : (col_low->column())) {
        res_insert = col_bsh.insert(cell.key());
        if( !res_insert.second ) { col_bsh.erase(res_insert.first); } //1+1 = 0
        //o.w. insertion has succeeded.
      }
      //If col_bsh is entirely reduced, i.e. col_bsh == \emptyset.
      if(col_bsh.empty())
      { 
        surjective_reflection_diamond(zzsh, chains_in_F, chains_in_H); 
        return; 
      }
      //Else, keep reducing.
      col_low = &(*lowidx_to_matidx_[*(col_bsh.rbegin())]); //curr low index col
      paired_idx = col_low->paired_col_;//col with which col_low is paired
    }
  }


/** \brief Computes an injective diamond in the zigzag module, by inserting a new
  * column for the chain zzsh - \sum col_h, for all col_h in chains_in_H, and a
  * new row for the simplex zzsh.
  */
  void injective_reflection_diamond ( Simplex_handle zzsh
                                    , std::vector< matrix_chain * > & chains_in_H )
  { //Compute the chain   zzsh + \sum col_h, for col_h \in chains_in_H
    std::set< Simplex_key > col_bsh;
    std::pair< typename std::set< Simplex_key >::iterator, bool > res_insert;
    //produce the sum of all col_h in chains_in_H
    for( matrix_chain *idx_h : chains_in_H ) {
      for(auto &cell : (idx_h->column()) ) {
        res_insert = col_bsh.insert(cell.key());
        if( !res_insert.second ) { col_bsh.erase(res_insert.first); }
      }
    }
    //create a new cycle (in F) sigma - \sum col_h    
    matrix_.emplace_front(cpx_->key(zzsh), col_bsh.begin(), col_bsh.end(), 
                          lowidx_to_matidx_);
    //Update the map 'index idx -> chain with lowest index idx' in matrix_
    auto chain_it = matrix_.begin();
    lowidx_to_matidx_[cpx_->key(zzsh)] = chain_it;
  }

/** The vector chains_in_F is sorted by decreasing lowest index values in the
  * columns corresponding to the chains, due to its computation in the reduction of
  * \partial zzsh in forward_arrow(...). It is equivalent to decreasing death index
  * order w.r.t. the <d ordering.
  */
  void surjective_reflection_diamond( Simplex_handle zzsh
                                    , std::vector< matrix_chain * > & chains_in_F
                                    , std::vector< matrix_chain * > & chains_in_H )
  { //fp is the largest death index for <=d
    //Set col_fp: col_fp <- col_f1+...+col_fp (now in G); preserves lowest idx
    auto chain_fp = *(chains_in_F.begin()); //col_fp, with largest death <d index.
   
    for(auto other_col_it = chains_in_F.begin()+1;
             other_col_it != chains_in_F.end(); ++other_col_it)
    { plus_equal_column(chain_fp, chain_fp->column(), (*other_col_it)->column()); }
    //doesn't change the lowest idx as chain_fp has maximal lowest idx of all

    //chains_in_F is ordered, from .begin() to end(), by decreasing lowest_idx_. The
    //lowest_idx_ is also the death of the chain in the right suffix of the 
    //filtration (all backward arrows). Consequently, the chains in F are ordered by 
    //decreasing death for <d.
    //Pair the col_fi, i = 1 ... p-1, according to the reflection diamond principle
    //Order the fi by reverse birth ordering <=_b           
    auto cmp_birth = [this](Simplex_key k1, Simplex_key k2)->bool
    { return birth_ordering_.reverse_birth_order(k1,k2); };//true iff b(k1) >b b(k2)

    //available_birth: for all i by >d value of the d_i, 
    //contains at step i all b_j, j > i, and maybe b_i if not stolen
    std::set< Simplex_key, decltype(cmp_birth) > available_birth(cmp_birth);
    //for f1 to f_{p} (i by <=d), insertion in available_birth_to_fidx sorts by >=b
    for(auto &chain_f : chains_in_F) { available_birth.insert(chain_f->birth()); }

#ifdef _DEBUG_ZIGZAG_PERSISTENCE_
  if(available_birth.find(chain_fp->birth()) == available_birth.end()) {
    std::cout << "Miss chain_fp in available_birth when performing surjective_diamond \n"; }
#endif

    auto maxb_it = available_birth.begin();//max birth cycle
    auto maxb = *maxb_it; //max birth value, for persistence diagram
    available_birth.erase(maxb_it); //remove max birth cycle (stolen)

    auto last_modified_chain_it = chains_in_F.rbegin();

    //consider all death indices by increasing <d order i.e., increasing lowest_idx_
    for(auto chain_f_it  = chains_in_F.rbegin(); //by increasing death order <d
            *chain_f_it != chain_fp; ++chain_f_it )//chain_fp=*begin() has max death
    { //find which reduced col has this birth
      auto birth_it = available_birth.find((*chain_f_it)->birth());
      if(birth_it == available_birth.end()) //birth is not available. *chain_f_it
      { //must become the sum of all chains in F with smaller death index.
        //this gives as birth the maximal birth of all chains with strictly larger 
        //death <=> the maximal availabe death.
        //Let c_1 ... c_f be the chains s.t. <[c_1+...+c_f]> is the kernel and
        // death(c_i) >d death(c_i-1). If the birth of c_i is not available, we set
        //c_i <- c_i + c_i-1 + ... + c_1, which is [c_i + c_i-1 + ... + c_1] on 
        //the right (of death the maximal<d death(c_i)), and is [c_i + c_i-1 + ... + 
        //c_1] + kernel = [c_f + c_f-1 + ... + c_i+1] on the left (of birth the max<b
        //of the birth of the c_j, j>i  <=> the max<b available birth).
        //N.B. some of the c_k, k<i, ahve already been modified to be equal to 
        //c_k + c_k-1 + ... + c_1. The largest k with this property is maintained in 
        //last_modified_chain_it (no need to compute from scratch the full sum).

        //last_modified is equal to c_k+...+c_1, all c_j, i>j>k, are indeed c_j
        //set c_i <- c_i + (c_i-1) + ... + (c_k+1) + (c_k + ... + c_1)
        for(auto chain_passed_it = last_modified_chain_it;//all with smaller <d death
                 chain_passed_it != chain_f_it;  ++chain_passed_it)
        {
          plus_equal_column( (*chain_f_it), (*chain_f_it)->column()
                           , (*chain_passed_it)->column() );
        }
        last_modified_chain_it = chain_f_it;//new cumulated c_i+...+c_1
        //remove the max available death
        auto max_avail_b_it = available_birth.begin();//max because order by deacr <b
        Simplex_key max_avail_b = *max_avail_b_it;//max available birth

#ifdef _DEBUG_ZIGZAG_PERSISTENCE_
      bool there_is_zero = false;
      for(auto it = chain_f_it+1; it != chains_in_F.rend(); ++it) {
        if( cmp_birth((*it)->birth(), max_avail_b) ) {// >b
          std::cout << "Mismatch max birth availabe\n";
        }
        else {
          if( !(cmp_birth(max_avail_b,(*it)->birth())) ) { there_is_zero = true; }
        }
      }
      if(!there_is_zero) { std::cout << "Max birth unmatched\n"; }
#endif

      (*chain_f_it)->assign_birth(max_avail_b); //give new birth
      available_birth.erase(max_avail_b_it); //remove birth from availability
    }
    else { available_birth.erase(birth_it); } //birth not available anymore, do not
                                              //modify *chain_f_it.
  }
  //Compute the new column zzsh + \sum col_h, for col_h in chains_in_H
  std::set< Simplex_key > col_bsh;
  std::pair< typename std::set< Simplex_key >::iterator, bool > res_insert;
  for(auto other_col : chains_in_H) 
  { //Compute (\sum col_h) in a set
    for(auto &cell : (other_col->column())) 
    {
      res_insert = col_bsh.insert(cell.key());
      if( !res_insert.second ) { col_bsh.erase(res_insert.first); } //1+1=0
    }
  }
  //Create and insert (\sum col_h) + sigma (in H, paired with chain_fp) in matrix_
  matrix_.emplace_front(cpx_->key(zzsh), chain_fp, col_bsh.begin(), col_bsh.end(), lowidx_to_matidx_);
  //record that the chain with lowest index key(zzsh) is the one just created
  auto chain_it = matrix_.begin();
  lowidx_to_matidx_[cpx_->key(zzsh)] = chain_it;//new row

  chain_fp->assign_paired_chain( &(*chain_it) );//pair chain_fp with the new chain
  chain_fp->assign_birth(-1); //now belongs to G now -> right interval [m-1,g]

  //Update persistence diagram with left interval [fil(b_max) ; fil(m))
  persistence_diagram_.emplace_back( cpx_->dimension(zzsh)-1
                                   , maxb 
                                   , cpx_->key(zzsh)-1);//
}

  //cpx_->key(zzsh) is the key of the simplex we remove, not a new one
  void backward_arrow( Simplex_handle zzsh )
  { 
#ifdef _PROFILING_ZIGZAG_PERSISTENCE_  
    ++_num_backward_arrow_; 
#endif
    //maintain the <=b order
    birth_ordering_.add_birth_backward(num_arrow_);
    //column whose key is the one of the removed simplex
    auto curr_col_it = lowidx_to_matidx_.find(cpx_->key(zzsh));
    //corresponding chain
    matrix_chain * curr_col    = &(*(curr_col_it->second));
    //Record all columns that get affected by the transpositions, i.e., have a coeff
    std::vector< matrix_chain * > modified_columns;//in the row of idx key(zzsh)
    for(auto & hcell : (curr_col->row_)) {
      modified_columns.push_back(hcell.self_chain_);
    }
    //Sort by left-to-right order in the matrix_ (no order maintained in rows)
    std::stable_sort( modified_columns.begin(),modified_columns.end()
                    , [](matrix_chain *mc1, matrix_chain *mc2)
    { return mc1->lowest_idx_ < mc2->lowest_idx_;} );

#ifdef _PROFILING_ZIGZAG_PERSISTENCE_ 
//increment (or assign 1) the value map[num of arrow transpose] = times it happened
  size_t entry_length, exit_length; 
  auto result = _stats_num_arrow_trans_per_backarrow_.emplace(modified_columns.size()-1,1);
  if(!(result.second)) {(result.first)->second += 1;}
  //  entry_length = 0; for(auto c : *(curr_col->column())) { ++entry_length; }
  entry_length = curr_col->column().size(); 
  auto start = std::chrono::high_resolution_clock::now();
#endif

#ifdef _DEBUG_ZIGZAG_PERSISTENCE_
    if( (*(modified_columns.begin()))->lowest_idx_ != cpx_->key(zzsh)) { std::cout << "Issue in backward arrow\n";}
    if(curr_col != *(modified_columns.begin())) {std::cout << "ERROR back arr\n";}
#endif

    //Modifies the pointer curr_col, not the other one.
    for(auto other_col_it = modified_columns.begin()+1;
        other_col_it != modified_columns.end(); ++other_col_it) { 
      curr_col = arrow_transposition_case_study(curr_col, *other_col_it);
    }

#ifdef _PROFILING_ZIGZAG_PERSISTENCE_    
  auto end = std::chrono::high_resolution_clock::now();
  exit_length = curr_col->column().size(); 

  //timings according to number of iterations of arrow transpose
  //0 times and _X_ times
  if(modified_columns.size() > 1) {
    timing_num_arrow_larger_than_1 += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(); }
  else {
    timing_num_arrow_smaller_than_1 += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(); }
  //
  if(modified_columns.size() > _X_-1) {
    timing_num_arrow_larger_than_X += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(); }
  else {
    timing_num_arrow_smaller_than_X += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(); }
  //timings when according to length of starting vector
  if(entry_length > _Y_-1) {
    timing_entry_length_larger_than_Y += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(); }
  else {
    timing_entry_length_smaller_than_Y += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(); }
  //timings when according to length of exit vector
  if(exit_length > _Z_-1) {
    timing_exit_length_larger_than_Z += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(); }
  else {
    timing_exit_length_smaller_than_Z += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(); }
#endif    

    //curr_col points to the column to remove by restriction of K to K-{\sigma}
    if( curr_col->paired_col_ == nullptr ) { // in F 
      int dim_zzsh = cpx_->dimension(zzsh);
      if(dim_zzsh < dim_max_) { //don't record intervals of max dim
        persistence_diagram_.emplace_back( dim_zzsh
                                         , curr_col->birth()
                                         , num_arrow_ -1);
      }
    }
    else { //in H    -> paired with c_g, that now belongs to F now
      curr_col->paired_col_->assign_paired_chain(nullptr);
      curr_col->paired_col_->assign_birth(num_arrow_); //closed interval
    }

    //cannot be in G
#ifdef _DEBUG_ZIGZAG_PERSISTENCE_
    if(curr_col->birth_ == -1) {  std::cout << "Error restrict a G column. \n"; } 
#endif

    matrix_.erase(curr_col_it->second);
    lowidx_to_matidx_.erase(curr_col_it);
  }

/* Exchanges members of matrix_chains, except the column_ pointer. Modify
 * also the lowidx_to_matidx_ data structure, considering that the matrix chains
 * also exchange their lowest_idx_. Specifically, it is called by 
 * arrow_transposition_case_study when:  
 * c_s has originally birth b_s and low idx s, and c_t has birth b_t and low idx t
 * however, c_s becomes c_s <- c_s+c_t with b_t <b b_s, and has SAME birth b_s but
 * NEW lowest index t. c_t remains the same but, because the positions of s and t
 * are transposed in the filtration, c_t keeps its birth b_t but has NEW lowest 
 * idx s.
 *
 * Note that Cells in the matrix store a pointer to their matrix_chain_.
 * Consequently, exchanging columns would require to update all such pointers. That
 * is why we avoid doing it, and prefer exchanging all other attributes.
 */
  void exchange_lowest_indices_chains( matrix_chain * curr_col//c_s+c_t (prev. c_s)
                                     , matrix_chain * other_col )//c_t
  { 
    
#ifdef _DEBUG_ZIGZAG_PERSISTENCE_
    auto b_s = curr_col->birth(); auto b_t = other_col->birth();
    auto s = curr_col->lowest_idx_; auto t = other_col->lowest_idx_;
#endif

    //lowidx_to_matidx_[i]==matrix_chain* whose lowest index is i
    auto it_s = lowidx_to_matidx_.find(curr_col->lowest_idx_);
    auto it_t = lowidx_to_matidx_.find(other_col->lowest_idx_);

    std::swap(it_s->second, it_t->second);//swap matrix_chain* in lowidx_to_matidx_
    std::swap(curr_col->row_, other_col->row_);//swap associated row of lowest idx
    std::swap(curr_col->lowest_idx_, other_col->lowest_idx_);//swap lowest idx.

    //check
#ifdef _DEBUG_ZIGZAG_PERSISTENCE_
    if(curr_col->lowest_idx_ != t ||
       other_col->lowest_idx_ != s ||
       &*(lowidx_to_matidx_.find(s)->second) != other_col ||
       &*(lowidx_to_matidx_.find(t)->second) != curr_col ||
       curr_col->row_.begin()->key_ != t ||
       other_col->row_.begin()->key_ != s ||
       curr_col->birth() != b_s ||
       other_col->birth() != b_t ) 
    {std::cout << "Some error in exchange lowest idx.\n";}
#endif

  }

/**
 * Permutes s and t, s goes up, whose insertions are adjacent, i.e., the following
 * transformation (from (a) to (b)) in the filtration:
 * from (a) ... \leftrightarrow K \leftarrow ... \leftarrow K' U {s,t} \leftarrow K' U {s} \leftarrow K' \leftarrow ...,
 * where K' is at matrix index i, K' U {s} at matrix index i+1 and K' U {s,t} at 
 * matrix index i+2,
 *
 * to (b)   ... \leftrightarrow K \leftarrow ... \leftarrow K' U {s,t} \leftarrow K' U {t} \leftarrow K' \leftarrow ...,
 *
 * and the chain c_t has a non-trivial coefficient for s, i.e., 
 * the bloc matrix gives (and becomes):
 *                                 c_t                  c_t
 *                                  +                    +
 *      c_s c_t                    c_s c_s              c_s c_t
 *   s   1   1                  t   1   0            t   1   1
 *   t   0   1      --> either  s   0   1      or    s   0   1
 *
 * By construction, s is a simplex that we want to remove in the complex K. It is 
 * consequently maximal in K, and all complexes between K and K' U {s} in filtration
 * (a).
 *
 * If c_s and c_t are both cycles (in F)that, before the permutation, are carried by 
 * respectively the closed intervals [b_s, i+1] and [b_t, i+2], then the sum 
 * c_s + c_t is a cycle carried by the interval 
 *                                          [max<b {b_s,b_t} , max<d {i,i+1} = i+1]. 
 *
 * If c_s and c_t are both chains in H, paired with c_gs and c_gt (in G) resp.,
 * where c_gs is carried by [i;gs] and c_gt is carried by [i+1;gt] before 
 * permutation, then c_gt+c_gs is carried by 
 * [max<b {i+1,i}=i+1 ; max<d {gs,gt} = max {gs,gt}] because, all arrows being 
 * backward the max<b birth is the leftmost, and the max<d death is the leftmost.
 *
 * We have \partial(c_s+c_t) = c_gs+c_gt. Because both c_s and c_t contain s in their
 * sum, c_s+c_t (in Z/2Z) contains t but not s in its sum, and all other simplices 
 * are in K. Consequently, the chain exists in any complex containing K U {t} as 
 * subcomplex.
 *
 * Note that because all arrows are backward on the right side of the quiver
 * ... \leftrightarrow K \leftarrow ... \leftarrow K U {s,t} \leftarrow K U {t} \leftarrow K \leftarrow ..., we always have 
 * i+1 >d i (i+1 \leftarrow i backward).
 * If j \leftarrow  ...   \leftarrow k are both birth indices on the right part of the quiver (all 
 * backward arrows) then systematically k <b j.
 * If b \leftrightarrow ... \leftrightarrow K \leftarrow ... \leftarrow j \leftarrow ... are both birth indices, with j on the right
 * part of the quiver (all backward arrows), and b in the prefix, then 
 * systematically j <b b. 
 *
 * Because s is maximal in K, none of c_s or c_t, that have a non-trivial 
 * coefficient for s, can belong to G because cycles in G are the boundary of some 
 * chain in H. 
 *
 * We get the following cases:
 * c_s | c_t in:
 *  F  |  F   keep (c_s+c_t), c_x: such that c_x has the min birth of c_s and c_t. 
 *                                 Both chains are cycles in F.
 *  F  |  H   keep (c_s+c_t), c_s: c_s+c_t still in H (boundary is unchanged) and 
 *                                 c_s still in F
 *  H  |  F   keep (c_s+c_t), c_t: c_s+c_t still in H (boundary is unchanged) and 
 *                                 c_t still in F
 *  H  |  H   keep (c_s+c_t), c_x: let c_s be paired with c_gs in G and c_t be 
 *            paired with c_gt in G. Then (c_s+c_t) is paired with c_gs+c_gt in G, *            of death index max<d d_gs,d_gt. With only backward arrow, the maximal 
 *            death for <d is the leftmost (i.e., == to the highest key of c_gs or 
 *            c_gt in the matrix), and the maximal birth for <b is the leftmost.
 *            Because c_s+c_t exists in any complex containing K U {t}, c_gs+c_gt is
 *            killed by the insertion of t before and after permutation of arrows.
 *            In particular, [c_gs+c_gt] has the same lifespan as the cycle c_gs or
 *            c_gt with the maximal death for <d. 
 *          
 *            c_x is the chain c_s or c_t that is paired with the chain in G with 
 *            smaller death index in <d. We also update c_gy <- c_gs+c_gt, where c_gy
 *            is the cycle c_gs or c_gt with larger death index for <d (now paired 
 *            with c_gs+c_gt in H).
 *
 * Returns the new value of curr_col we continue with, i.e., the chain whose lowest 
 * index is the one of s, the simplex we are percolating left in the filtration, 
 * after permutation of arrows.
 */
  matrix_chain * arrow_transposition_case_study( matrix_chain * curr_col//c_s
                                               , matrix_chain * other_col )//c_t
  {

#ifdef _PROFILING_ZIGZAG_PERSISTENCE_
		  ++_num_arrow_trans_case_study_;
#endif

#ifdef _DEBUG_ZIGZAG_PERSISTENCE_
    if(curr_col->inG() || other_col->inG()) {
      std::cout << "ERROR, call arrow_transposition on a G column.\n";
    }
#endif


    //c_s has low idx s and c_t low idx t
    if(curr_col->inF()) 
    {//case F x *
      if(other_col->inH()) { //                                    case F x H
        plus_equal_column( other_col, other_col->column()//c_t <- c_s+c_t still in H
                         , curr_col->column() );//(birth -2) and low idx t

        #ifdef _PROFILING_ZIGZAG_PERSISTENCE_ 
          ++_num_arrow_trans_case_study_plus_equal_col_;
          ++case8;
        #endif

        return curr_col;
      }//end case F x H
      else //                                                      case F x F
      { //in F x F:           c_s+c_t has max<=b birth between b_t and b_s:
        if(birth_ordering_.birth_order(curr_col->birth(), other_col->birth())) 
        { //max<=b is the birth of other_col i.e., b_s <b b_t
          plus_equal_column( other_col, other_col->column()//c_t <- c_s+c_t of birth
                           , curr_col->column() );//b_t and lowest idx t. (same) 
          //c_s still has birth b_s (minimal) and lowest idx s

          #ifdef _PROFILING_ZIGZAG_PERSISTENCE_ 
            ++_num_arrow_trans_case_study_plus_equal_col_;
            ++case10;
          #endif

          return curr_col;//continue with c_s of smaller birth b_s and lowest idx s
        }//endif
        else 
        { //max<=b is the birth of curr_col, i.e., b_t <b b_s 
          plus_equal_column( curr_col, curr_col->column()//c_s <- c_s+c_t of birth
                           , other_col->column() );//b_s and of NEW lowest idx t
          //now c_t has (same) birth b_t (minimal) but NEW lowest idx s, so
          //exchange lowest_idx, the rows, and update lowidx_to_matidx structure
          exchange_lowest_indices_chains(curr_col, other_col);

          #ifdef _PROFILING_ZIGZAG_PERSISTENCE_ 
            ++_num_arrow_trans_case_study_plus_equal_col_;
            ++case11;
          #endif

          return other_col;//continue with c_t of (smaller) birth b_t and low idx s
        }//end else
      }//end case F x F 
    }//end case F x *
    else {//curr_col->inH() == true,                                case H x *
      if(other_col->inH()) {//                                      case H x H
        //Case H x H, c_s+c_t paired w/ c_gs+c_gt, of death 
        //max<d { key(c_gs),key(c_gt) } == usual max { key(c_gs),key(c_gt) }:
        auto curr_p_col  = curr_col->paired_col_; //c_s paired with c_gs, death d_gs
        auto other_p_col = other_col->paired_col_;//c_t paired with c_gt, death d_gt
        if( curr_p_col->lowest_idx_ < other_p_col->lowest_idx_)//<=> d_gs <d d_gt
        {
          plus_equal_column( other_p_col, other_p_col->column()//c_gt <- c_gs+c_gt,
                           , curr_p_col->column() );//of death d_gt, low idx d_gt 
            //(same because bigger), paired with c_s+c_t (now &c_t, updated below)
          plus_equal_column( other_col, other_col->column()//c_t <- c_t+c_s, still 
                           , curr_col->column() );//in H, low idx t (same)

          #ifdef _PROFILING_ZIGZAG_PERSISTENCE_ 
            ++_num_arrow_trans_case_study_plus_equal_col_;
            ++_num_arrow_trans_case_study_plus_equal_col_;
            ++case1; 
          #endif

          return curr_col;//continue with c_s, paired with c_gs of min death d_gs
        }
        else 
        {// d_gt <d d_gs
          plus_equal_column( curr_p_col, curr_p_col->column()//c_gs <- c_gs+c_gt, 
                           , other_p_col->column() );//of death d_gs, low idx d_gs 
            //(same because bigger), paired with c_s+c_t (now &c_s, updated below) 
          plus_equal_column( curr_col, curr_col->column()//c_s <- c_s+c_t, of NEW 
                           , other_col->column());//low idx t (still in H)
          //now c_s is still in H (birth -2) but has NEW lowest idx t, and c_t has 
          //low idx s after transposition.
          //exchange lowest_idx, the rows, and update lowidx_to_matidx structure
          exchange_lowest_indices_chains(curr_col, other_col);

          #ifdef _PROFILING_ZIGZAG_PERSISTENCE_ 
            ++_num_arrow_trans_case_study_plus_equal_col_;
            ++_num_arrow_trans_case_study_plus_equal_col_;
            ++case2;
          #endif

          return other_col; //continue with c_t, paired w. c_g' of min death g'
        }
      }//end case H x H
      else {//other_col->inF() == true,                             case H x F
        plus_equal_column( curr_col, curr_col->column() //c_s <- c_s+c_t still in H,
                         , other_col->column());       //(birth -2) and NEW low idx t
        //now c_t, still in F, has (same) birth b_t but NEW lowest idx s, so
        //exchange lowest_idx, the rows, and update lowidx_to_matidx structure
        exchange_lowest_indices_chains(curr_col, other_col);

        #ifdef _PROFILING_ZIGZAG_PERSISTENCE_ 
          ++_num_arrow_trans_case_study_plus_equal_col_;
          ++case3;
        #endif

        return other_col; //continue with c_t, still in F, of birth b_t and low idx s
      }
    }
  }


public:
  /** \brief Returns the index persistence diagram as an std::list of intervals.
   */
  std::list< interval_t > & index_persistence_diagram() 
  { return persistence_diagram_; }

private:
  Complex                                           * cpx_; // complex
  int                                                 dim_max_;//max dim complex
//idx -> chain with lowest element at index idx in matrix_
  std::map< Simplex_key, typename std::list<matrix_chain>::iterator > 
                                                      lowidx_to_matidx_;
  //arbitrary order for the matrix chains
  std::list< matrix_chain >                            matrix_; // 0 ... m-1
  // birth_vector                                           birth_vector_; //<=b order
  birth_ordering                                      birth_ordering_;
  std::list< interval_t >                             persistence_diagram_;
  Simplex_key                                         num_arrow_; //current index
  // filtration_values stores consecutive pairs (i,f) , (j,f') with f != f',
  // meaning that all inserted simplices with key in [i;j-1] have filtration value f
  //i is the smallest simplex index whose simplex has filtration value f.
  std::vector< std::pair< Simplex_key, Filtration_value > > filtration_values_;


public:

 /** \brief Output the persistence diagram in ostream.
   *
   * The file format is the following:
   *    p1*...*pr   dim b d
   *
   * where "dim" is the dimension of the homological feature,
   * b and d are respectively the birth and death of the feature and
   * p1*...*pr is the product of prime numbers pi such that the homology
   * feature exists in homology with Z/piZ coefficients.
   */
    // void output_diagram(std::ostream& ostream = std::cout) {
    
    //   int num_intervals = 10;

    //   // std::cout << "Filtration values: ";
    //   // for(auto pp : filtration_values_) {
    //   //   std::cout << "[ " << pp.first << " ; " << pp.second << " ]  ";
    //   // } std::cout << std::endl;

    //   std::vector< interval_t > tmp_diag;
    //   tmp_diag.reserve(persistence_diagram_.size());
    //   for(auto bar : persistence_diagram_) {
    //     tmp_diag.emplace_back(bar.dim_,index_to_filtration(bar.b_),index_to_filtration(bar.d_));
    //   }
    //   cmp_intervals_by_length cmp;
    //   std::stable_sort(tmp_diag.begin(), tmp_diag.end(), cmp);

    //   if(tmp_diag.empty()) {return;}

    //   int curr_dim = tmp_diag.begin()->dim_;
    //   int curr_num_intervals = num_intervals;

    //   for(auto bar : tmp_diag) {
    //     if(curr_dim != bar.dim_) {
    //       std::cout << "----------------------------------------- dim " << bar.dim_
    //       << " \n";
    //       curr_num_intervals = num_intervals; curr_dim = bar.dim_;
    //     }
    //     if(curr_num_intervals > 0) {
    //       --curr_num_intervals;
    //       std::cout << bar.dim_ << "   " << bar.b_ << " " << bar.d_ <<
    //                                               "      " << bar.length() << " \n";
    //     }
    //   }
    // }

/** \brief Writes the persistence diagram in a file.
  *
  * The filtration values are given by the zigzag persistence iterator, that assigns
  * to any insertion or deletion of a simplex a filtration value ; we say that an 
  * arrow has a filtration value. Consider two consecutive arrows (insertion or 
  * deletion):
  * 
  * \f$$K_1 \leftrightarrow \K_2 \leftrightarrow \K_3\f$$
  * 
  * of filtration values \f$f_{1,2}\f$ and \f$f_{2,3}\f$ respectively.
  * 
  * If, the arrow \f$\K_2 \leftrightarrow \K_3\f$ leads to the creation of a new 
  * homology feature in \f$K_3\f$, it creates an interval 
  * \f$[f_{2,3}; \cdot]\f$ in the persistence diagram. 
  * If a homology feature is destroyed in \f$K_2\f$ by such arrow, it closes an 
  * interval \f$[\cdot ; f_{1,2}]\f$ in the persistence diagram.
  * 
  * For example, in an oscillating Rips zigzag filtration, if, in the following 
  * chunk of filtration:
  * 
  * \f$R_{\eta \varepsilon_i}(P_i) \rightarrow \cdots \leftarrow R_{\eta \varepsilon_{i+1}}(P_{i+1}),\f$ 
  * 
  * if anything is created by any of the arrows above, it leads to an interval  
  * \f$[\varepsilon_{i+1}; \cdot]\f$. If anything is destroyed by any of the arrows 
  * above, if leads to an interval \f$[\cdot;\varepsilon_i]\f$. Note that we may 
  * have \f$\varepsilon_i > \varepsilon_{i+1}\f$.
  *
  * The bars are ordered by decreasing length. 
  * 
  * @param[in] os, the output stream in which the diagram is written.
  * @param[in] shortest_interval, all intervals of lenght smaller or equal to 
  *                               this value are ignore. Default is 0.
  */
  void persistence_diagram( std::ostream& os 
                          , Filtration_value shortest_interval = 0.) {

    std::stable_sort(filtration_values_.begin(), filtration_values_.end(),
         []( std::pair< Simplex_key, Filtration_value > p1
           , std::pair< Simplex_key, Filtration_value > p2 )
           { return p1.first < p2.first; }
    );

    std::vector< interval_t > tmp_diag;
    tmp_diag.reserve(persistence_diagram_.size());
    for(auto bar : persistence_diagram_)
    {
      auto it_b = //lower_bound(x) returns leftmost y s.t. x <= y
        std::lower_bound( filtration_values_.begin(), filtration_values_.end()
              , std::pair<Simplex_key, Filtration_value>(bar.b_
                                        , std::numeric_limits<double>::infinity() )
              , []( std::pair<Simplex_key, Filtration_value> p1
                  , std::pair<Simplex_key, Filtration_value> p2) 
                  {
                    return p1.first < p2.first; 
                  }
        );
      //
      if(it_b == filtration_values_.end() || it_b->first > bar.b_) { --it_b; }

      auto it_d = //upper_bound(x) returns leftmost y s.t. x < y, or last
        std::upper_bound( filtration_values_.begin(), filtration_values_.end()
              , std::pair<Simplex_key, Filtration_value>(bar.d_
                                         , std::numeric_limits<double>::infinity() )
              , []( std::pair<Simplex_key, Filtration_value> p1
                  , std::pair<Simplex_key, Filtration_value> p2) 
                  {
                    return p1.first < p2.first; 
                  }
        );
      //discard interval strictly included between two consecutive indices
      --it_d;
      if( std::abs(it_b->second - it_d->second) > shortest_interval ) {
        tmp_diag.emplace_back(bar.dim_, it_b->second, it_d->second );
      }
    }
    cmp_intervals_by_length cmp;
    std::stable_sort(tmp_diag.begin(), tmp_diag.end(), cmp);

    os << "# dim  birth  death  [length]\n";
    for(auto bar : tmp_diag) {
      if(bar.b_ > bar.d_) { std::swap(bar.b_, bar.d_); }
      os << bar.dim_ << " " << bar.b_ << " " << bar.d_ <<
       " - [" << bar.length() << "] \n";
    }
  }

/** \brief Returns the persistence diagram.
  */
  std::vector< std::tuple<int,Filtration_value,Filtration_value> > 
    persistence_diagram(double shortest_interval = 0.) {

    std::stable_sort(filtration_values_.begin(), filtration_values_.end(),
         []( std::pair< Simplex_key, Filtration_value > p1
           , std::pair< Simplex_key, Filtration_value > p2 )
           { return p1.first < p2.first; }
    );

    auto persistence_diagram_copy(persistence_diagram_);
    for(auto &bar : persistence_diagram_copy) { bar.d_ += 1; }

    std::vector< std::tuple<int,Filtration_value,Filtration_value> > diag;
    diag.reserve(persistence_diagram_.size());
    for(auto bar : persistence_diagram_copy)
    {
      auto it_b = //lower_bound(x) returns leftmost y s.t. x <= y
        std::lower_bound( filtration_values_.begin(), filtration_values_.end()
              , std::pair<Simplex_key, Filtration_value>(bar.b_
                                        , std::numeric_limits<double>::infinity() )
              , []( std::pair<Simplex_key, Filtration_value> p1
                  , std::pair<Simplex_key, Filtration_value> p2) 
                  {
                    return p1.first < p2.first; 
                  }
        );
      //
      if(it_b == filtration_values_.end() || it_b->first > bar.b_) { --it_b; }

      auto it_d = //upper_bound(x) returns leftmost y s.t. x < y, or last
        std::upper_bound( filtration_values_.begin(), filtration_values_.end()
              , std::pair<Simplex_key, Filtration_value>(bar.d_
                                         , std::numeric_limits<double>::infinity() )
              , []( std::pair<Simplex_key, Filtration_value> p1
                  , std::pair<Simplex_key, Filtration_value> p2) 
                  {
                    return p1.first < p2.first; 
                  }
        );
      //discard interval strictly included between two consecutive indices
      --it_d;
      if(it_b->second != it_d->second) {
        if( std::abs(it_b->second - it_d->second) > shortest_interval ) {
          diag.emplace_back(bar.dim_, it_b->second, it_d->second );
        }
      }
    }
    //put lower value as birth
    for(auto &bar : diag) {
      if( std::get<1>(bar) > std::get<2>(bar) ) 
      { std::swap(std::get<1>(bar),std::get<2>(bar)); }
    }

    return diag;
  }

};

struct Zigzag_persistence_collist {
  static const bool searchable_column = false;
};

struct Zigzag_persistence_colset {
  static const bool searchable_column = true;
};

} //namespace zigzag_persistence

} //namespace Gudhi

#endif //ZIGZAG_PERSISTENCE_H_

/*  $Id$

    Author:        Willem Robert van Hage
    E-mail:        wrvhage@few.vu.nl
    WWW:           http://www.few.vu.nl/~wrvhage
    Copyright (C): 2009, Vrije Universiteit Amsterdam
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    As a special exception, if you link this library with other files,
    compiled with a Free Software compiler, to produce an executable, this
    library does not by itself cause the resulting executable to be covered
    by the GNU General Public License. This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/


#include "Index.h"


/*
 * -- DataStream --------------------------------------------
 */

class RTreePrologStream : public IDataStream
{
public:
  qid_t q;
  predicate_t p;
  term_t cache0;
  bool cached;
  RTreeIndex *index;
public:
  RTreePrologStream(const char* module, const char *goal,RTreeIndex* i) {
    cache0 = PL_new_term_refs(2);
    p = PL_predicate(goal,2,module);
    cached = false;
    q = PL_open_query(NULL, PL_Q_NORMAL, p, cache0);
    index = i;
    index->bulkload_tmp_id_cnt = 0;
  }
  ~RTreePrologStream() {
    PL_cut_query(q);
    //    index->bulkload_tmp_id_cnt = -1;
  }
  virtual IData* getNext() {
    if (!cached && !PL_next_solution(q)) return NULL;
    cached = false;
    IShape *s = index->interpret_shape((term_t)(cache0+1));
    Region r;
    s->getMBR(r);
    char* uri;
    PL_get_atom_chars((term_t)cache0,&uri);
    id_type id = index->get_uri_id(uri);
    if (id == -1) id = index->get_new_id(uri);
    index->storeShape(id,s);
#ifdef DEBUG
    cout << uri << " size " << strlen(uri)+1 << " shape " << r << " id " << id << endl;
#endif
    RTree::Data* next = new RTree::Data((size_t)strlen(uri)+1, (byte*)uri, r, id);
    return next;
  }
  virtual bool hasNext() throw (NotSupportedException)
  {
    if (cached) return true;
    if (PL_next_solution(q)) {
      cached = true;
      return true;
    }
    cached = false;
    return false;
  }
  
  virtual size_t size() throw (NotSupportedException)
  {
    throw NotSupportedException("Operation not supported.");
    return 0;
  }
  
  virtual void rewind() throw (NotSupportedException)  
  {
    cerr << "rewinding a RTreePrologStream does nothing" << endl;
    //    throw NotSupportedException("Operation not supported.");
  }
};


#define INSERT 1
#define DELETE 0
#define QUERY 2

class RTreeFileStream : public IDataStream
{
private:
  unsigned int count;
public:
  RTreeFileStream(string inputFile) : count(0), m_pNext(0)
  {
    m_fin.open(inputFile.c_str());
    
    if (! m_fin)
      throw IllegalArgumentException("Input file not found.");
    
    id_type id;
    uint32_t op;
    double low[2], high[2];
    string uri;

    m_fin >> op >> id >> low[0] >> low[1] >> high[0] >> high[1] >> uri;
    
    #ifdef DEBUG
    cout << "read URI [" << uri << "]\n";
    #endif

    if (m_fin.good())
      {
        if (op != INSERT)
          throw IllegalArgumentException(
                                         "The data input should contain insertions only."
                                         );
        
        Region r = Region(low, high, 2);
        char* uri_str = new char[uri.size()+1];
        strcpy(uri_str,uri.c_str());
        m_pNext = new RTree::Data(uri.size()+1, (byte*)uri_str, r, id);
        count++;
      }
  }
  
  virtual ~RTreeFileStream()
  {
    if (m_pNext != 0) delete m_pNext;
  }
  
  virtual IData* getNext()
  {
    if (m_pNext == 0) {
      cout << "read " << count << " entries" << endl;
      return 0;
    }    
    RTree::Data* ret = m_pNext;
    m_pNext = 0;
    
    id_type id;
    uint32_t op;
    double low[2], high[2];
    string uri;

    m_fin >> op >> id >> low[0] >> low[1] >> high[0] >> high[1] >> uri;
    
    #ifdef DEBUG
    cout << "read URI [" << uri << "]\n";
    #endif

    if (m_fin.good())
      {
        if (op != INSERT)
          throw IllegalArgumentException(
                                         "The data input should contain insertions only."
                                         );
        
        Region r = Region(low, high, 2);
        char* uri_str = new char[uri.size()+1];
        strcpy(uri_str,uri.c_str());
        m_pNext = new RTree::Data(uri.size()+1, (byte*)uri_str, r, id);
        count++;
      } 

    return ret;
  }
  
  virtual bool hasNext() throw (NotSupportedException)
  {
    return (m_pNext != 0);
  }
  
  virtual size_t size() throw (NotSupportedException)
  {
    throw NotSupportedException("Operation not supported.");
  }
  
  virtual void rewind() throw (NotSupportedException)
  {
    id_type id;
    uint32_t op;
    double low[2], high[2];
    string uri;

    if (m_pNext != 0)
      {
        delete m_pNext;
        m_pNext = 0;
      }
    
    m_fin.seekg(0, std::ios::beg);
    m_fin >> op >> id >> low[0] >> low[1] >> high[0] >> high[1] >> uri;
    
    #ifdef DEBUG
    cout << "rewound URI [" << uri << "]\n";
    #endif
    
    if (m_fin.good())
      {
        if (op != INSERT)
          throw IllegalArgumentException(
                                         "The data input should contain insertions only."
                                         );
        
        Region r = Region(low, high, 2);
        char* uri_str = new char[uri.size()+1];
        strcpy(uri_str,uri.c_str());
        m_pNext = new RTree::Data(uri.size()+1, (byte*)uri_str, r, id);
        count--;
      }
  }
  
  std::ifstream m_fin;
  RTree::Data* m_pNext;
};



/*
 * -- RTreeIndex -------------------------------------
 */

RTreeIndex::RTreeIndex(const char* indexname) : utilization(0.7), nodesize(4),  diskfile(NULL), file(NULL), tree(NULL) { 
    baseName = new string(indexname);
    bulkload_tmp_id_cnt = -1;
}

RTreeIndex::RTreeIndex(const char* indexname, double util, int nodesz) : diskfile(NULL), file(NULL), tree(NULL) {
    baseName = new string(indexname);
    utilization = util;
    nodesize = nodesz;
    bulkload_tmp_id_cnt = -1;
}


RTreeIndex::~RTreeIndex() {
  this->clear_tree();
  delete baseName;
}

void RTreeIndex::clear_tree() {
  if (tree != NULL) {
    delete tree;
    tree = NULL;
  }
  if (file != NULL) { 
    delete file;
    file = NULL;
  }
  if(diskfile != NULL) {
    delete diskfile;
    diskfile = NULL;
  }
  uri_id_map.clear();
  map<id_type,IShape*>::iterator id_shape_iter;
  for( id_shape_iter = id_shape_map.begin(); id_shape_iter != id_shape_map.end(); ++id_shape_iter ) {
    GEOSShape *s = dynamic_cast<GEOSShape*>(id_shape_iter->second);
    if (s != NULL) {
      global_factory->destroyGeometry(s->g);
      s->g = NULL;
    }    
    delete id_shape_iter->second;
  }
  id_shape_map.clear();
}

void RTreeIndex::storeShape(id_type id,IShape *s) {
  id_shape_map[id] = s;
}
IShape* RTreeIndex::getShape(id_type id) {
  map<id_type,IShape*>::iterator iter = id_shape_map.find(id);
  if (iter == id_shape_map.end()) return NULL;
  return iter->second;
}

id_type  RTreeIndex::get_new_id(const char* uri) {
  id_type id = -1;
  if (bulkload_tmp_id_cnt != -1) { // we're bulkloading
    id = bulkload_tmp_id_cnt++;
    /*
    string *q = new string(uri);
    uri_id_map[*q] = id;
    delete q;
    */
    uri_id_map[uri] = id;
  } else {
    if (tree == NULL) return -1;
    IStatistics* stats;
    tree->getStatistics(&stats);
    id = stats->getNumberOfData();
    /*
    string *q = new string(uri);
    uri_id_map[*q] = id;
    delete q;
    */
    uri_id_map[uri] = id;

    delete stats;
  }
  return id;
}

id_type   RTreeIndex::get_uri_id(const char* uri) {
  //  map<string,id_type>::iterator iter = uri_id_map.find(string(uri));
  map<const char*,id_type>::iterator iter = uri_id_map.find(uri);
  if (iter == uri_id_map.end()) return -1;
  return iter->second;
}
  
bool
RTreeIndex::bulk_load(const char* module,const char* goal,size_t dimensionality) {
  if (dimensionality > 3 || dimensionality < 1) {
    cerr << "only dimensionality from 1 to 3 supported, not " << dimensionality << endl;
    return false;
  }
  RTreePrologStream stream(module,goal,this); // assuming of the shape 'somepred(URI,Shape)'

  // FIXME: add a nice customization interface that allows you to choose between disk and memory storage
  // and to set the parameters of the disk store and buffer
  diskfile = StorageManager::createNewDiskStorageManager(*baseName, 32);
  //diskfile = StorageManager::createNewMemoryStorageManager();
  file = StorageManager::createNewRandomEvictionsBuffer(*diskfile, 4096, true);
  id_type indexIdentifier;
  tree = RTree::createAndBulkLoadNewRTree(RTree::BLM_STR, 
                                          stream, *file, utilization,
                                          nodesize, nodesize, dimensionality, 
                                          SpatialIndex::RTree::RV_RSTAR, indexIdentifier);
  return tree->isIndexValid();
}


IShape* RTreeIndex::interpret_shape(term_t shape) {
  // only supports points and boxes now
  PlTerm shape_term(shape);

  if (shape_term.name() == ATOM_point) {

    double point[shape_term.arity()];
    for (int i = 1; i <= shape_term.arity(); i++) {
      point[i-1] = (double)shape_term[i];
    }
    //  return new Point(point,shape_term.arity());
    GEOSPoint *p = new GEOSPoint(point,shape_term.arity()); // testing GEOS points
    #ifdef DEBUG
    cout << "made point " << p << endl;
    #endif
    return p;

  } else if (shape_term.name() == ATOM_polygon) {

    if (shape_term.arity() != 1) {
      cout << "arity not 1: polygon must have one argument, a list containing a list of points representing the shell, and an second argument, a list containing lists of points representing the holes" << endl;
      return NULL;
    } else if (!PL_is_list(shape_term[1])) {
      cout << "first argument not a list: polygon must have one argument, a list containing a list of points representing the shell, and an second argument, a list containing lists of points representing the holes" << endl;
      return NULL;    
    }
    geos::geom::CoordinateSequence *cl = new geos::geom::CoordinateArraySequence();
    PlTail linearrings(shape_term[1]);
    PlTerm ring;
    linearrings.next(ring);
    PlTail shell(ring);
    PlTerm pt;
    while (shell.next(pt)) {
      if (pt.name() != ATOM_point) {
        cerr << "polygon shell contains non-point" << endl;
        return NULL;
      }
      size_t dim = pt.arity();
      if (dim == 2) {
        cl->add(geos::geom::Coordinate((double)pt[1],(double)pt[2]));
      } else if (dim == 1) {
        cl->add(geos::geom::Coordinate((double)pt[1]));
      } else if (dim == 3) {
        cl->add(geos::geom::Coordinate((double)pt[1],(double)pt[2],(double)pt[3]));
      } else {
        cerr << dim << " dimensional points not supported" << endl;
      }
    }
    // assuming linear ring is already closed
    //    cl->add(cl->getAt(0)); 
    geos::geom::LinearRing *lr = global_factory->createLinearRing(*cl);
    vector<geos::geom::Geometry*> *holes = new vector<geos::geom::Geometry*>;
    while (linearrings.next(ring)) {
      geos::geom::CoordinateSequence *hcl = new geos::geom::CoordinateArraySequence();
      PlTail hole(ring);
      PlTerm hpt;
      while (hole.next(hpt)) {
        if (hpt.name() != ATOM_point) {
          cerr << "polygon hole contains non-point" << endl;
          return NULL;
        }
        size_t dim = hpt.arity();
        if (dim == 2) {
          hcl->add(geos::geom::Coordinate((double)hpt[1],(double)hpt[2]));
        } else if (dim == 1) {
          hcl->add(geos::geom::Coordinate((double)hpt[1]));
        } else if (dim == 3) {
          hcl->add(geos::geom::Coordinate((double)hpt[1],(double)hpt[2],(double)hpt[3]));
        } else {
          cerr << dim << " dimensional points not supported" << endl;
        }
      }
      // assuming linear ring is already closed
      //      hcl->add(cl->getAt(0));
      geos::geom::LinearRing *hlr = global_factory->createLinearRing(hcl);
      holes->push_back(hlr);
    }
    geos::geom::Polygon *p = global_factory->createPolygon(lr,holes);
    p->normalize();
    GEOSPolygon *poly = new GEOSPolygon(*p);
    delete p;
    delete cl;
    return poly;

  } else if (shape_term.name() == ATOM_box) {

    #ifdef DEBUG
    cout << "reading box" << endl;
    #endif
    if (shape_term.arity() != 2) {
      cerr << "MBR dimensionality must be 2 (low and high), but is " << shape_term.arity() << endl;
      return NULL;
    }
    int dim = shape_term.arity();
    double low_point[dim], high_point[dim];
    if (shape_term[1].name() != ATOM_point) return NULL;
    for (int i = 1; i <= dim; i++) {
      low_point[i-1] = (double)shape_term[1][i];
    }
    if (shape_term[2].name() != ATOM_point) return NULL;
    for (int i = 1; i <= dim; i++) {
      high_point[i-1] = (double)shape_term[2][i];
    }

    geos::geom::Polygon *box;
    if (dim == 2) {
      geos::geom::CoordinateSequence *cl = new geos::geom::CoordinateArraySequence();
      cl->add(geos::geom::Coordinate(low_point[0], low_point[1]));
      cl->add(geos::geom::Coordinate(low_point[0], high_point[1]));
      cl->add(geos::geom::Coordinate(high_point[0], high_point[1]));
      cl->add(geos::geom::Coordinate(high_point[0], low_point[1]));
      cl->add(geos::geom::Coordinate(low_point[0], low_point[1]));
      geos::geom::LinearRing *lr = global_factory->createLinearRing(*cl);
      box = global_factory->createPolygon(lr, NULL);
      box->normalize();
      delete cl;
    } else if (dim == 1) {
      geos::geom::CoordinateSequence *cl = new geos::geom::CoordinateArraySequence();
      cl->add(geos::geom::Coordinate(low_point[0]));
      cl->add(geos::geom::Coordinate(high_point[0]));
      geos::geom::LinearRing *lr = global_factory->createLinearRing(*cl);
      box = global_factory->createPolygon(lr, NULL);
      box->normalize();
      delete cl;
    } else if (dim == 3) {
      cerr << "3d box regions not implemented yet" << endl;
      return NULL;
    } else {
      cerr << dim << " dimensional regions not supported" << endl;
      return NULL;
    }
    GEOSPolygon *poly = new GEOSPolygon(*box);
    global_factory->destroyGeometry(box);
    return poly;
  } else {
    cerr << "shape type \"" << (char*)shape_term.name() << "\" unsupported" << endl;
    return NULL;
  }
  return NULL;
}

void RTreeIndex::create_tree(size_t dimensionality) {
  RTreeIndex::create_tree(dimensionality,utilization,nodesize);
}
void RTreeIndex::create_tree(size_t dimensionality, double util, int nodesz) {
  utilization = util;
  nodesize = nodesz;
  diskfile = StorageManager::createNewDiskStorageManager(*baseName, 4096);
  file = StorageManager::createNewRandomEvictionsBuffer(*diskfile, 10, false);
  tree = RTree::createNewRTree(*file, utilization, nodesize, nodesize, dimensionality, SpatialIndex::RTree::RV_RSTAR, indexIdentifier);
}

bool RTreeIndex::insert_single_object(const char* uri,term_t shape_term) {
  IShape *shape;
  id_type id;
  if ((shape = RTreeIndex::interpret_shape(shape_term)) == NULL) {
    cerr << "could not interpret shape" << endl;
    return FALSE;
  }
  if (tree == NULL) {
    size_t dimensionality = shape->getDimension();
    clear_tree();
    RTreeIndex::create_tree(dimensionality,utilization,nodesize);
  }
  if ((id = get_new_id(uri)) == -1) {
    cerr << "could not generate new ID" << endl;
    return FALSE;
  }
  try { 
    tree->insertData((size_t)strlen(uri)+1, (byte*)uri, *shape, id);    
  } catch (...) {
    return FALSE;
  }
  storeShape(id,shape);
  return TRUE;
}

bool RTreeIndex::delete_single_object(const char* uri,term_t shape_term) {
  IShape *shape;
  id_type id;
  if ((shape = RTreeIndex::interpret_shape(shape_term)) == NULL) {
    cerr << "could not interpret shape" << endl;
    return FALSE;
  }
  if (tree == NULL) {
    size_t dimensionality = shape->getDimension();
    clear_tree();
    RTreeIndex::create_tree(dimensionality,utilization,nodesize);
  }
  if ((id = get_uri_id(uri)) == -1) {
    cerr << "could not find ID for " << uri << " in " << *baseName << endl;
    delete shape;
    return FALSE;
  }
  bool rv = tree->deleteData(*shape, id);
  delete shape;
  return rv;
}

bool RTreeIndex::load_from_file(const char* filename) {
  clear_tree();
  bool ret = true;
  try
    {  
      //      delete baseName;
      //      baseName = new string(filename);

      // Create a new storage manager with the provided base name and a 4K page size.
        diskfile = StorageManager::createNewDiskStorageManager(*baseName, 4096);
      file = StorageManager::createNewRandomEvictionsBuffer(*diskfile, 10, false);
      // applies a main memory random buffer on top of the persistent storage manager
      // (LRU buffer, etc can be created the same way).
      
      RTreeFileStream stream(filename);
      
      cout << "creating with nodesize " << nodesize << endl;
      
      // Create and bulk load a new RTree with dimensionality 2, using "file" as
      // the StorageManager and the RSTAR splitting policy.
      tree = RTree::createAndBulkLoadNewRTree
        ( RTree::BLM_STR, stream, *file, 
          utilization, nodesize, nodesize, 2, 
          SpatialIndex::RTree::RV_RSTAR, indexIdentifier );
      
      cerr << *tree;
      cerr << "Buffer hits: " << file->getHits() << endl;
      cerr << "Index ID: " << indexIdentifier << endl;
      
      ret = tree->isIndexValid();
      if (ret == false) cerr << "ERROR: Structure is invalid!" << endl;
      else cerr << "The stucture seems O.K." << endl;
    }
  catch (Exception& e)
    {
      cerr << "******ERROR******" << endl;
      std::string s = e.what();
      cerr << s << endl;
      return -1;
    }
  catch (...)
    {
      cerr << "******ERROR******" << endl;
      cerr << "other exception" << endl;
      return -1;
    }
  return ret;
}


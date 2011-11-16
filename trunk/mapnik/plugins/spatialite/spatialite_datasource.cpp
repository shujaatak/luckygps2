/*****************************************************************************
 * 
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2010 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/
// $Id$

#include "sqlite_datasource.hpp"
#include "sqlite_featureset.hpp"

// mapnik
#include <mapnik/ptree_helpers.hpp>
#include <mapnik/sql_utils.hpp>

// to enable extent fallback hack
#include <mapnik/feature_factory.hpp>

// boost
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/make_shared.hpp>


using boost::lexical_cast;
using boost::bad_lexical_cast;

using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(sqlite_datasource)

using mapnik::box2d;
using mapnik::coord2d;
using mapnik::query;
using mapnik::featureset_ptr;
using mapnik::layer_descriptor;
using mapnik::attribute_descriptor;
using mapnik::datasource_exception;


sqlite_datasource::sqlite_datasource(parameters const& params, bool bind)
   : datasource(params),
     extent_(),
     extent_initialized_(false),
     type_(datasource::Vector),
     table_(*params_.get<std::string>("table","")),
     fields_(*params_.get<std::string>("fields","*")),
     metadata_(*params_.get<std::string>("metadata","")),
     geometry_table_(*params_.get<std::string>("geometry_table","")),
     geometry_field_(*params_.get<std::string>("geometry_field","")),
     // index_table_ defaults to "idx_{geometry_table_}_{geometry_field_}"
     index_table_(*params_.get<std::string>("index_table","")),
     // http://www.sqlite.org/lang_createtable.html#rowid
     key_field_(*params_.get<std::string>("key_field","")),
     row_offset_(*params_.get<int>("row_offset",0)),
     row_limit_(*params_.get<int>("row_limit",0)),
     desc_(*params_.get<std::string>("type"), *params_.get<std::string>("encoding","utf-8")),
     format_(mapnik::wkbGeneric)
{
    // TODO
    // - change param from 'file' to 'dbname'
    // - ensure that the supplied key_field is a valid "integer primary key"
    
    boost::optional<std::string> file = params_.get<std::string>("file");
    if (!file) throw datasource_exception("Sqlite Plugin: missing <file> parameter");

    if (table_.empty()) {
        throw mapnik::datasource_exception("Sqlite Plugin: missing <table> parameter");
    }
    
    if (bind)
    {
        this->bind();
    }
}

void sqlite_datasource::parse_attachdb(std::string const& attachdb) const
{
    boost::char_separator<char> sep(",");
    boost::tokenizer<boost::char_separator<char> > tok(attachdb, sep);
    
    // The attachdb line is a comma sparated list of
    //    [dbname@]filename
    for (boost::tokenizer<boost::char_separator<char> >::iterator beg = tok.begin(); 
         beg != tok.end(); ++beg)
    {
        std::string const& spec(*beg);
        size_t atpos=spec.find('@');
        
        // See if it contains an @ sign
        if (atpos==spec.npos) {
            throw datasource_exception("attachdb parameter has syntax dbname@filename[,...]");
        }
        
        // Break out the dbname and the filename
        std::string dbname = boost::trim_copy(spec.substr(0, atpos));
        std::string filename = boost::trim_copy(spec.substr(atpos+1));
        
        // Normalize the filename and make it relative to dataset_name_
        if (filename.compare(":memory:") != 0) {

            boost::filesystem::path child_path(filename);
            
            // It is a relative path.  Fix it.
            if (!child_path.has_root_directory() && !child_path.has_root_name()) {
                boost::filesystem::path absolute_path(dataset_name_);

                // support symlinks
                #if (BOOST_FILESYSTEM_VERSION == 3)
                if (boost::filesystem::is_symlink(absolute_path))
                {
                    absolute_path = boost::filesystem::read_symlink(absolute_path);
                }

                filename = boost::filesystem::absolute(absolute_path.parent_path()/filename).string();

                #else
                if (boost::filesystem::is_symlink(absolute_path))
                {
                    //cannot figure out how to resolve on in v2 so just print a warning
                    std::clog << "###Warning: '" << absolute_path.string() << "' is a symlink which is not supported in attachdb\n";
                }

                filename = boost::filesystem::complete(absolute_path.branch_path()/filename).normalize().string();

                #endif
            }
        }
        
        // And add an init_statement_
        init_statements_.push_back("attach database '" + filename + "' as " + dbname);
    }
}

void sqlite_datasource::bind() const
{
    if (is_bound_) return;
    
    boost::optional<std::string> file = params_.get<std::string>("file");
    if (!file) throw datasource_exception("Sqlite Plugin: missing <file> parameter");
    
    boost::optional<std::string> key_field_name = params_.get<std::string>("key_field");
    if (key_field_name) {
        std::string const& key_field_string = *key_field_name;
        if (key_field_string.empty()) {
            key_field_ = "rowid";
        } else {
            key_field_ = key_field_string;
        }
    }
    else
    {
        key_field_ = "rowid";
    }
    
    boost::optional<std::string> wkb = params_.get<std::string>("wkb_format");
    if (wkb)
    {
        if (*wkb == "spatialite")
            format_ = mapnik::wkbSpatiaLite;  
    }

    multiple_geometries_ = *params_.get<mapnik::boolean>("multiple_geometries",false);
    use_spatial_index_ = *params_.get<mapnik::boolean>("use_spatial_index",true);
    has_spatial_index_ = false;
    using_subquery_ = false;

    boost::optional<std::string> ext  = params_.get<std::string>("extent");
    if (ext) extent_initialized_ = extent_.from_string(*ext);

    boost::optional<std::string> base = params_.get<std::string>("base");
    if (base)
        dataset_name_ = *base + "/" + *file;
    else
        dataset_name_ = *file;

    // Populate init_statements_
    //   1. Build attach database statements from the "attachdb" parameter
    //   2. Add explicit init statements from "initdb" parameter
    // Note that we do some extra work to make sure that any attached
    // databases are relative to directory containing dataset_name_.  Sqlite
    // will default to attaching from cwd.  Typicaly usage means that the
    // map loader will produce full paths here.
    boost::optional<std::string> attachdb = params_.get<std::string>("attachdb");
    if (attachdb) {
       parse_attachdb(*attachdb);
    }
    
    boost::optional<std::string> initdb = params_.get<std::string>("initdb");
    if (initdb) {
       init_statements_.push_back(*initdb);
    }

    if (!boost::filesystem::exists(dataset_name_))
        throw datasource_exception("Sqlite Plugin: " + dataset_name_ + " does not exist");
          
    dataset_ = new sqlite_connection (dataset_name_);

    // Execute init_statements_
    for (std::vector<std::string>::const_iterator iter=init_statements_.begin(); iter!=init_statements_.end(); ++iter)
    {
#ifdef MAPNIK_DEBUG
        std::clog << "Sqlite Plugin: Execute init sql: " << *iter << std::endl;
#endif
        dataset_->execute(*iter);
    }
    
    if(geometry_table_.empty())
    {
        geometry_table_ = mapnik::table_from_sql(table_);
    }

    // should we deduce column names and types using PRAGMA?
    bool use_pragma_table_info = true;
    
    if (table_ != geometry_table_)
    {
        // if 'table_' is a subquery then we try to deduce names
        // and types from the first row returned from that query
        using_subquery_ = true;
        use_pragma_table_info = false;
    }

    if (!use_pragma_table_info)
    {
        std::ostringstream s;
        s << "SELECT " << fields_ << " FROM (" << table_ << ") LIMIT 1";

        boost::scoped_ptr<sqlite_resultset> rs(dataset_->execute_query(s.str()));
        if (rs->is_valid() && rs->step_next())
        {
            for (int i = 0; i < rs->column_count (); ++i)
            {
               const int type_oid = rs->column_type (i);
               const char* fld_name = rs->column_name (i);
               switch (type_oid)
               {
                  case SQLITE_INTEGER:
                     desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Integer));
                     break;
                     
                  case SQLITE_FLOAT:
                     desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Double));
                     break;
                     
                  case SQLITE_TEXT:
                     desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::String));
                     break;
                     
                  case SQLITE_NULL:
                     // sqlite reports based on value, not actual column type unless
                     // PRAGMA table_info is used so here we assume the column is a string
                     // which is a lesser evil than altogether dropping the column
                     desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::String));

                  case SQLITE_BLOB:
                        if (geometry_field_.empty() && 
                             (boost::algorithm::icontains(fld_name,"geom") ||
                              boost::algorithm::icontains(fld_name,"point") ||
                              boost::algorithm::icontains(fld_name,"linestring") ||
                              boost::algorithm::icontains(fld_name,"polygon"))
                            )
                            geometry_field_ = std::string(fld_name);
                     break;
                     
                  default:
#ifdef MAPNIK_DEBUG
                     std::clog << "Sqlite Plugin: unknown type_oid=" << type_oid << std::endl;
#endif
                     break;
                }
            }
        }
        else
        {
            // if we do not have at least a row and
            // we cannot determine the right columns types and names 
            // as all column_type are SQLITE_NULL
            // so we fallback to using PRAGMA table_info
            use_pragma_table_info = true;
        }
    }

    if (key_field_ == "rowid")
        desc_.add_descriptor(attribute_descriptor("rowid",mapnik::Integer));
    
    if (use_pragma_table_info)
    {
        std::ostringstream s;
        s << "PRAGMA table_info(" << geometry_table_ << ")";
        boost::scoped_ptr<sqlite_resultset> rs(dataset_->execute_query(s.str()));
        bool found_table = false;
        while (rs->is_valid() && rs->step_next())
        {
            found_table = true;
            // TODO - support unicode strings?
            const char* fld_name = rs->column_text(1);
            std::string fld_type(rs->column_text(2));
            boost::algorithm::to_lower(fld_type);

            // see 2.1 "Column Affinity" at http://www.sqlite.org/datatype3.html
            if (geometry_field_.empty() && 
                       (
                       (boost::algorithm::icontains(fld_name,"geom") ||
                        boost::algorithm::icontains(fld_name,"point") ||
                        boost::algorithm::icontains(fld_name,"linestring") ||
                        boost::algorithm::icontains(fld_name,"polygon"))
                       ||
                       (boost::algorithm::contains(fld_type,"geom") ||
                        boost::algorithm::contains(fld_type,"point") ||
                        boost::algorithm::contains(fld_type,"linestring") ||
                        boost::algorithm::contains(fld_type,"polygon"))
                       )
                    )
                    geometry_field_ = std::string(fld_name);
            else if (boost::algorithm::contains(fld_type,"int"))
            {
                desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Integer));
            }
            else if (boost::algorithm::contains(fld_type,"text") ||
                     boost::algorithm::contains(fld_type,"char") ||
                     boost::algorithm::contains(fld_type,"clob"))
            {
                desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::String));
            }
            else if (boost::algorithm::contains(fld_type,"real") ||
                     boost::algorithm::contains(fld_type,"float") ||
                     boost::algorithm::contains(fld_type,"double"))
            {
                desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Double));
            }
            else if (boost::algorithm::contains(fld_type,"blob") && !geometry_field_.empty())
            {
                 desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::String));
            }
#ifdef MAPNIK_DEBUG
            else
            {
                // "Column Affinity" says default to "Numeric" but for now we pass..
                //desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Double));
                std::clog << "Sqlite Plugin: column '" << std::string(fld_name) << "' unhandled due to unknown type: " << fld_type << std::endl;
            }
#endif
        }
        if (!found_table)
        {
            std::ostringstream s;
            s << "Sqlite Plugin: could not query table '" << geometry_table_ << "' ";
            if (using_subquery_)
                s << " from subquery '" << table_ << "' ";
            s << "using 'PRAGMA table_info(" << geometry_table_  << ")' ";
            std::string sq_err = std::string(sqlite3_errmsg(*(*dataset_)));
            if (sq_err != "unknown error")
                s << ": " << sq_err;
            throw datasource_exception(s.str());
        }
    }

    if (geometry_field_.empty()) {
        throw datasource_exception("Sqlite Plugin: cannot detect geometry_field, please supply the name of the geometry_field to use.");
    }

    if (index_table_.size() == 0) {
        // Generate implicit index_table name - need to do this after
        // we have discovered meta-data or else we don't know the column name
        index_table_ = "\"idx_" + mapnik::unquote_sql2(geometry_table_) + "_" + geometry_field_ + "\"";
    }
    
    if (use_spatial_index_)
    {
        std::ostringstream s;
        s << "SELECT pkid,xmin,xmax,ymin,ymax FROM " << index_table_;
        s << " LIMIT 0";
        if (dataset_->execute_with_code(s.str()) == SQLITE_OK)
        {
            has_spatial_index_ = true;
        }
        #ifdef MAPNIK_DEBUG
        else
        {
            std::clog << "SQLite Plugin: rtree index lookup did not succeed: '" << sqlite3_errmsg(*(*dataset_)) << "'\n";
        }
        #endif
    }

    if (metadata_ != "" && !extent_initialized_)
    {
        std::ostringstream s;
        s << "SELECT xmin, ymin, xmax, ymax FROM " << metadata_;
        s << " WHERE LOWER(f_table_name) = LOWER('" << geometry_table_ << "')";
        boost::scoped_ptr<sqlite_resultset> rs(dataset_->execute_query(s.str()));
        if (rs->is_valid() && rs->step_next())
        {
            double xmin = rs->column_double (0);
            double ymin = rs->column_double (1);
            double xmax = rs->column_double (2);
            double ymax = rs->column_double (3);

            extent_.init (xmin,ymin,xmax,ymax);
            extent_initialized_ = true;
        }
    }
    
    if (!extent_initialized_ && has_spatial_index_)
    {
        std::ostringstream s;
        s << "SELECT MIN(xmin), MIN(ymin), MAX(xmax), MAX(ymax) FROM " 
        << index_table_;
        boost::scoped_ptr<sqlite_resultset> rs(dataset_->execute_query(s.str()));
        if (rs->is_valid() && rs->step_next())
        {
            if (!rs->column_isnull(0)) {
                try 
                {
                    double xmin = lexical_cast<double>(rs->column_double(0));
                    double ymin = lexical_cast<double>(rs->column_double(1));
                    double xmax = lexical_cast<double>(rs->column_double(2));
                    double ymax = lexical_cast<double>(rs->column_double(3));
                    extent_.init (xmin,ymin,xmax,ymax);
                    extent_initialized_ = true;
    
                }
                catch (bad_lexical_cast &ex)
                {
                    std::clog << boost::format("SQLite Plugin: warning: could not determine extent from query: %s\nError was: '%s'\n") % s.str() % ex.what() << std::endl;
                }
            }
        }    
    }

#ifdef MAPNIK_DEBUG
    if (!has_spatial_index_
        && use_spatial_index_ 
        && using_subquery_
        && key_field_ == "rowid"
        && !boost::algorithm::icontains(table_,"rowid")) {
       // this is an impossible situation because rowid will be null via a subquery
       std::clog << "Sqlite Plugin: WARNING: spatial index usage will fail because rowid is not present in your subquery. You have 4 options: 1) Add rowid into your select statement, 2) alias your primary key as rowid, 3) supply a 'key_field' value that references the primary key of your spatial table, or 4) avoid using a spatial index by setting 'use_spatial_index'=false";
    }
#endif
    
    // final fallback to gather extent
    if (!extent_initialized_ || !has_spatial_index_) {
                
        std::ostringstream s;
        
        s << "SELECT " << geometry_field_ << "," << key_field_
          << " FROM " << geometry_table_;
        if (row_limit_ > 0) {
            s << " LIMIT " << row_limit_;
        }
        if (row_offset_ > 0) {
            s << " OFFSET " << row_offset_;
        }

#ifdef MAPNIK_DEBUG
        std::clog << "Sqlite Plugin: " << s.str() << std::endl;
#endif

        boost::shared_ptr<sqlite_resultset> rs(dataset_->execute_query(s.str()));
        
        // spatial index sql
        std::string spatial_index_sql = "create virtual table " + index_table_ 
            + " using rtree(pkid, xmin, xmax, ymin, ymax)";
        std::string spatial_index_insert_sql = "insert into " + index_table_
            + " values (?,?,?,?,?)" ;
        sqlite3_stmt* stmt = 0;

        if (use_spatial_index_) {    
            dataset_->execute(spatial_index_sql);
            int rc = sqlite3_prepare_v2 (*(*dataset_), spatial_index_insert_sql.c_str(), -1, &stmt, 0);
            if (rc != SQLITE_OK)
            {
               std::ostringstream index_error;
               index_error << "Sqlite Plugin: auto-index table creation failed: '"
                 << sqlite3_errmsg(*(*dataset_)) << "' query was: " << spatial_index_insert_sql;
               throw datasource_exception(index_error.str());
            }
        }

        bool first = true;
        while (rs->is_valid() && rs->step_next())
        {
            int size;
            const char* data = (const char *) rs->column_blob (0, size);
            if (data) {
                // create a tmp feature to be able to parse geometry
                // ideally we would not have to do this.
                // see: http://trac.mapnik.org/ticket/745
                mapnik::feature_ptr feature(mapnik::feature_factory::create(0));
                mapnik::geometry_utils::from_wkb(feature->paths(),data,size,multiple_geometries_,format_);
                mapnik::box2d<double> const& bbox = feature->envelope();
                if (bbox.valid()) {
                    extent_initialized_ = true;
                    if (first) {
                        first = false;
                        extent_ = bbox;
                    } else {
                        extent_.expand_to_include(bbox);
                    }

                    // index creation
                    if (use_spatial_index_) {
                        const int type_oid = rs->column_type(1);
                        if (type_oid != SQLITE_INTEGER) {
                            std::ostringstream type_error;
                            type_error << "Sqlite Plugin: invalid type for key field '"
                                << key_field_ << "' when creating index '" << index_table_ 
                                << "' type was: " << type_oid << "";
                            throw datasource_exception(type_error.str());
                        }
    
                        sqlite_int64 pkid = rs->column_integer64(1);
                        if (sqlite3_bind_int64(stmt, 1 , pkid ) != SQLITE_OK)
                        {
                            throw datasource_exception("invalid value for for key field while generating index");
                        }
                        if ((sqlite3_bind_double(stmt, 2 , bbox.minx() ) != SQLITE_OK) ||
                           (sqlite3_bind_double(stmt, 3 , bbox.maxx() ) != SQLITE_OK) ||
                           (sqlite3_bind_double(stmt, 4 , bbox.miny() ) != SQLITE_OK) ||
                           (sqlite3_bind_double(stmt, 5 , bbox.maxy() ) != SQLITE_OK)) {
                               throw datasource_exception("invalid value for for extent while generating index");
                           }

                        int res = sqlite3_step(stmt);
                        if (res != SQLITE_DONE) {
                            std::ostringstream s;
                            s << "SQLite Plugin: inserting bbox into rtree index failed: "
                                 << "error code " << sqlite3_errcode(*(*dataset_)) << ": '"
                                 << sqlite3_errmsg(*(*dataset_)) << "' query was: " << spatial_index_insert_sql;
                            throw datasource_exception(s.str());
                        }
                        sqlite3_reset(stmt);
                    }
                }
                else {
                    std::ostringstream index_error;
                    index_error << "SQLite Plugin: encountered invalid bbox at '"
                        << key_field_ << "' == " << rs->column_integer64(1);
                    throw datasource_exception(index_error.str());
                }
            }
        }
        
        int res = sqlite3_finalize(stmt);
        if (res != SQLITE_OK)
        {
            throw datasource_exception("auto-indexing failed: set use_spatial_index=false to disable auto-indexing and avoid this error");
        }
        
    }

    if (!extent_initialized_) {
        std::ostringstream s;
        s << "Sqlite Plugin: extent could not be determined for table '" 
          << geometry_table_ << "' and geometry field '" << geometry_field_ << "'"
          << " because an rtree spatial index is missing or empty."
          << " - either set the table 'extent' or create an rtree spatial index";
        throw datasource_exception(s.str());
    }
    
    is_bound_ = true;
}

sqlite_datasource::~sqlite_datasource()
{
    if (is_bound_) 
    {
        delete dataset_;
    }
}

std::string sqlite_datasource::name()
{
   return "sqlite";
}

int sqlite_datasource::type() const
{
   return type_;
}

box2d<double> sqlite_datasource::envelope() const
{
   if (!is_bound_) bind();
   return extent_;
}

layer_descriptor sqlite_datasource::get_descriptor() const
{
   if (!is_bound_) bind();
   return desc_;
}

featureset_ptr sqlite_datasource::features(query const& q) const
{
   if (!is_bound_) bind();
   if (dataset_)
   {
        mapnik::box2d<double> const& e = q.get_bbox();

        std::ostringstream s;
        
        s << "SELECT " << geometry_field_ << "," << key_field_;
        std::set<std::string> const& props = q.property_names();
        std::set<std::string>::const_iterator pos = props.begin();
        std::set<std::string>::const_iterator end = props.end();
        while (pos != end)
        {
            s << ",\"" << *pos << "\"";
            ++pos;
        }       
        
        s << " FROM "; 
        
        std::string query (table_);
        
        /* todo
          throw if select * and key_field == rowid?
          or add schema support so sqlite throws
        */
        
        if (has_spatial_index_)
        {
           /* 
              Use rtree to limit record id's to a given bbox
              then a btree to pull the records for those ids.        
           */
           std::ostringstream spatial_sql;
           spatial_sql << std::setprecision(16);
           spatial_sql << " WHERE " << key_field_ << " IN (SELECT pkid FROM " << index_table_;
           spatial_sql << " WHERE xmax>=" << e.minx() << " AND xmin<=" << e.maxx() ;
           spatial_sql << " AND ymax>=" << e.miny() << " AND ymin<=" << e.maxy() << ")";
           if (boost::algorithm::ifind_first(query, "WHERE"))
           {
              boost::algorithm::ireplace_first(query, "WHERE", spatial_sql.str() + " AND ");
           }
           else if (boost::algorithm::ifind_first(query, geometry_table_))  
           {
              boost::algorithm::ireplace_first(query, table_, table_ + " " + spatial_sql.str());
           }
        }
        
        s << query ;
        
        if (row_limit_ > 0) {
            s << " LIMIT " << row_limit_;
        }

        if (row_offset_ > 0) {
            s << " OFFSET " << row_offset_;
        }

#ifdef MAPNIK_DEBUG
        std::clog << "Sqlite Plugin: table_: " << table_ << "\n\n";
        std::clog << "Sqlite Plugin: query:" << s.str() << "\n\n";
#endif

        boost::shared_ptr<sqlite_resultset> rs(dataset_->execute_query(s.str()));

        return boost::make_shared<sqlite_featureset>(rs, desc_.get_encoding(), format_, multiple_geometries_, using_subquery_);
   }

   return featureset_ptr();
}

featureset_ptr sqlite_datasource::features_at_point(coord2d const& pt) const
{
   if (!is_bound_) bind();

   if (dataset_)
   {
        // TODO - need tolerance
        mapnik::box2d<double> const e(pt.x,pt.y,pt.x,pt.y);

        std::ostringstream s;
        s << "SELECT " << geometry_field_ << "," << key_field_;
        std::vector<attribute_descriptor>::const_iterator itr = desc_.get_descriptors().begin();
        std::vector<attribute_descriptor>::const_iterator end = desc_.get_descriptors().end();
        while (itr != end)
        {
            std::string fld_name = itr->get_name();
            if (fld_name != key_field_)
                s << ",\"" << itr->get_name() << "\"";
            ++itr;
        }
        
        s << " FROM "; 
        
        std::string query (table_); 
        
        if (has_spatial_index_)
        {
           std::ostringstream spatial_sql;
           spatial_sql << std::setprecision(16);
           spatial_sql << " WHERE " << key_field_ << " IN (SELECT pkid FROM " << index_table_;
           spatial_sql << " WHERE xmax>=" << e.minx() << " AND xmin<=" << e.maxx() ;
           spatial_sql << " AND ymax>=" << e.miny() << " AND ymin<=" << e.maxy() << ")";
           if (boost::algorithm::ifind_first(query, "WHERE"))
           {
              boost::algorithm::ireplace_first(query, "WHERE", spatial_sql.str() + " AND ");
           }
           else if (boost::algorithm::ifind_first(query, geometry_table_))  
           {
              boost::algorithm::ireplace_first(query, table_, table_ + " " + spatial_sql.str());
           }
        }
        
        s << query ;
        
        if (row_limit_ > 0) {
            s << " LIMIT " << row_limit_;
        }

        if (row_offset_ > 0) {
            s << " OFFSET " << row_offset_;
        }

#ifdef MAPNIK_DEBUG
        std::clog << "Sqlite Plugin: " << s.str() << std::endl;
#endif

        boost::shared_ptr<sqlite_resultset> rs(dataset_->execute_query(s.str()));

        return boost::make_shared<sqlite_featureset>(rs, desc_.get_encoding(), format_, multiple_geometries_, using_subquery_);
   }
      
   return featureset_ptr();
}


/*****************************************************************************
 * 
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2007 Artem Pavlenko
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
//$Id$

// mapnik
#include <mapnik/global.hpp>
#include <mapnik/datasource.hpp>
#include <mapnik/box2d.hpp>
#include <mapnik/geometry.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/feature_layer_desc.hpp>
#include <mapnik/wkb.hpp>
#include <mapnik/unicode.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/sql_utils.hpp>
#include <string.h>

// ogr
#include "sqlite_featureset.hpp"

using mapnik::query;
using mapnik::box2d;
using mapnik::CoordTransform;
using mapnik::Feature;
using mapnik::feature_ptr;
using mapnik::geometry_utils;
using mapnik::transcoder;
using mapnik::feature_factory;

sqlite_featureset::sqlite_featureset(boost::shared_ptr<sqlite_resultset> rs,
                                     std::string const& encoding,
                                     mapnik::wkbFormat format,
                                     bool multiple_geometries,
                                     bool using_subquery)
   : rs_(rs),
     tr_(new transcoder(encoding)),
     format_(format),
     multiple_geometries_(multiple_geometries),
     using_subquery_(using_subquery)
{
}

sqlite_featureset::~sqlite_featureset() {}

void sqlite_dequote(char *z){
  char quote;                     /* Quote character (if any ) */

  quote = z[0];
  if( quote=='[' || quote=='\'' || quote=='"' || quote=='`' ){
    int iIn = 1;                  /* Index of next byte to read from input */
    int iOut = 0;                 /* Index of next byte to write to output */

    /* If the first byte was a '[', then the close-quote character is a ']' */
    if( quote=='[' ) quote = ']';  

    while( z[iIn] ){
      if( z[iIn]==quote ){
        if( z[iIn+1]!=quote ) break;
        z[iOut++] = quote;
        iIn += 2;
      }else{
        z[iOut++] = z[iIn++];
      }
    }
    z[iOut] = '\0';
  }
}



feature_ptr sqlite_featureset::next()
{
    if (rs_->is_valid () && rs_->step_next ())
    {
        int size;
        const char* data = (const char *) rs_->column_blob (0, size);
        if (!data)
            return feature_ptr();
        int feature_id = rs_->column_integer (1);   

        feature_ptr feature(feature_factory::create(feature_id));
        geometry_utils::from_wkb(feature->paths(),data,size,multiple_geometries_,format_);
        
        for (int i = 2; i < rs_->column_count (); ++i)
        {
            const int type_oid = rs_->column_type (i);
            const char* fld_name = rs_->column_name(i);

            if (!fld_name)
                continue;

            if (!using_subquery_)
            {
                switch (type_oid)
                {
                  case SQLITE_INTEGER:
                  {
                     boost::put(*feature,fld_name,rs_->column_integer (i));
                     break;
                  }
                  
                  case SQLITE_FLOAT:
                  {
                     boost::put(*feature,fld_name,rs_->column_double (i));
                     break;
                  }
                  
                  case SQLITE_TEXT:
                  {
                     int text_size;
                     const char * data = rs_->column_text(i,text_size);
                     UnicodeString ustr = tr_->transcode(data,text_size);
                     boost::put(*feature,fld_name,ustr);
                     break;
                  }
    
                  case SQLITE_NULL:
                  {
                     boost::put(*feature,fld_name,mapnik::value_null());
                     break;                 
                  }
                  
                  case SQLITE_BLOB:
                     break;
                     
                  default:
    #ifdef MAPNIK_DEBUG
                     std::clog << "Sqlite Plugin: unhandled type_oid=" << type_oid << std::endl;
    #endif
                     break;
                }
            }
            else
            {
                // subqueries in sqlite lead to field double quoting which we need to strip
                char fld_name2[strlen(fld_name)];
                strcpy(fld_name2,fld_name);
                sqlite_dequote(fld_name2);
                switch (type_oid)
                {
                  case SQLITE_INTEGER:
                  {
                     boost::put(*feature,fld_name2,rs_->column_integer (i));
                     break;
                  }
                  
                  case SQLITE_FLOAT:
                  {
                     boost::put(*feature,fld_name2,rs_->column_double (i));
                     break;
                  }
                  
                  case SQLITE_TEXT:
                  {
                     int text_size;
                     const char * data = rs_->column_text(i,text_size);
                     UnicodeString ustr = tr_->transcode(data,text_size);
                     boost::put(*feature,fld_name2,ustr);
                     break;
                  }
    
                  case SQLITE_NULL:
                  {
                     boost::put(*feature,fld_name2,mapnik::value_null());
                     break;                 
                  }
                  
                  case SQLITE_BLOB:
                     break;
                     
                  default:
    #ifdef MAPNIK_DEBUG
                     std::clog << "Sqlite Plugin: unhandled type_oid=" << type_oid << std::endl;
    #endif
                     break;
                }
            }
        }

        return feature;
    }

    return feature_ptr();
}


/*
  3dfier: takes 2D GIS datasets and "3dfies" to create 3D city models.

  Copyright (C) 2015-2018  3D geoinformation research group, TU Delft

  This file is part of 3dfier.

  3dfier is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  3dfier is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with 3difer.  If not, see <http://www.gnu.org/licenses/>.

  For any information or further details about the use of 3dfier, contact
  Hugo Ledoux
  <h.ledoux@tudelft.nl>
  Faculty of Architecture & the Built Environment
  Delft University of Technology
  Julianalaan 134, Delft 2628BL, the Netherlands
*/

//-- TODO: create the topo DS locally? to prevent cases where nodes on only in one polygon. Or pprepair before?
//-- TODO: write output all polygons once the tile completely containing is closed?
//-- TODO : how to make roads horizontal "in the width"? 

//-----------------------------------------------------------------------------

#include "yaml-cpp/yaml.h"
#include "definitions.h"
#include "io.h"
#include "TopoFeature.h"
#include "Map3d.h"
#include "boost/locale.hpp"
#include "boost/chrono.hpp"
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <map>

std::string VERSION = "0.9.8";

bool validate_yaml(const char* arg, std::set<std::string>& allowedFeatures);
int main(int argc, const char * argv[]);
std::string print_license();

int main(int argc, const char * argv[]) {
  auto startTime = boost::chrono::high_resolution_clock::now();
  boost::locale::generator gen;
  std::locale loc = gen("en_US.UTF-8");
  std::locale::global(loc);
  std::clog.imbue(loc);

  std::string licensewarning =
    "3dfier Copyright (C) 2015-2018  3D geoinformation research group, TU Delft\n"
    "This program comes with ABSOLUTELY NO WARRANTY.\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under certain conditions; for details run 3dfier with the '--license' option.\n";

  std::map<std::string,std::string> outputs;
  outputs["OBJ"]      = "";
  outputs["OBJ-NoID"] = "";
  outputs["CityGML"]  = "";
  outputs["CityGML-Multifile"]  = "";
  outputs["CityGML-IMGeo"]  = "";
  outputs["CityGML-IMGeo-Multifile"]  = "";
  outputs["CityJSON"] = "";
  outputs["CSV-BUILDINGS"] = "";
  outputs["CSV-BUILDINGS-MULTIPLE"] = "";
  outputs["CSV-BUILDINGS-ALL-Z"] = "";
  outputs["Shapefile"] = "";
  outputs["Shapefile-Multi"] = "";
  outputs["PostGIS"] = "";
  outputs["PostGIS-Multi"] = "";
  outputs["PostGIS-PDOK"] = "";
  outputs["GDAL"] = "";
  std::string f_yaml;
  try {
    namespace po = boost::program_options;
    po::options_description pomain("Allowed options");
    pomain.add_options()
      ("help",    "View all options")
      ("version", "View version")
      ("license", "View license")
      ("OBJ",       po::value<std::string>(&outputs["OBJ"]), "Output ")
      ("OBJ-NoID",  po::value<std::string>(&outputs["OBJ-NoID"]), "Output ")
      ("CityGML",   po::value<std::string>(&outputs["CityGML"]), "Output ")
      ("CityGML-Multifile", po::value<std::string>(&outputs["CityGML-Multifile"]), "Output ")
      ("CityGML-IMGeo", po::value<std::string>(&outputs["CityGML-IMGeo"]), "Output ")
      ("CityGML-IMGeo-Multifile", po::value<std::string>(&outputs["CityGML-IMGeo-Multifile"]), "Output ")
      ("CityJSON",  po::value<std::string>(&outputs["CityJSON"]), "Output ")
      ("CSV-BUILDINGS", po::value<std::string>(&outputs["CSV-BUILDINGS"]), "Output ")
      ("CSV-BUILDINGS-MULTIPLE", po::value<std::string>(&outputs["CSV-BUILDINGS-MULTIPLE"]), "Output ")
      ("CSV-BUILDINGS-ALL-Z", po::value<std::string>(&outputs["CSV-BUILDINGS-ALL-Z"]), "Output ")
      ("Shapefile", po::value<std::string>(&outputs["Shapefile"]), "Output ")
      ("Shapefile-Multi", po::value<std::string>(&outputs["Shapefile-Multi"]), "Output ")
      ("PostGIS", po::value<std::string>(&outputs["PostGIS"]), "Output ")
      ("PostGIS-Multi", po::value<std::string>(&outputs["PostGIS-Multi"]), "Output ")
      ("PostGIS-PDOK", po::value<std::string>(&outputs["PostGIS-PDOK"]), "Output ")
      ("GDAL", po::value<std::string>(&outputs["GDAL"]), "Output ")
    ;
    po::options_description pohidden("Hidden options");
    pohidden.add_options()
      ("yaml", po::value<std::string>(&f_yaml), "Input config YAML file")
    ;        
    po::positional_options_description popos;
    popos.add("yaml", -1);

    po::options_description poall;
    poall.add(pomain).add(pohidden);
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
              options(poall).positional(popos).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << "Usage: 3dfier config.yml --OBJ myoutput.obj" << std::endl;
      std::cout << "       3dfier config.yml --CityGML myoutput.gml" << std::endl;
      std::cout << pomain << std::endl;
      return 0;
    }
    if (vm.count("license")) {
      std::cout << print_license() << std::endl;
      return 0;
    }
    if (vm.count("version")) {
      std::cout << "3dfier " << VERSION << std::endl;
      std::cout << liblas::GetFullVersion() << std::endl;
      std::cout << "GDAL " << GDALVersionInfo("--version") << std::endl;
      //-- TODO : put here the date and/or the git-commit?
      return 0;
    }
    if (vm.count("yaml") == 0) {
      std::cerr << "ERROR: one YAML config file must be specified." << std::endl;
      std::cout << std::endl << pomain << std::endl;
      return 0;  
    }
    else {
      boost::filesystem::path yp(f_yaml);
      if (boost::filesystem::exists(yp) == false) {
        std::cerr << "ERROR: YAML file " << f_yaml << " doesn't exist." << std::endl;
        return 0;
      }
      if (yp.extension() != ".yml") {
        std::cerr << "ERROR: config file " << f_yaml << " if not *.yml" << std::endl;
        return 0;
      }
    }
    for (auto& output : vm) {
      if ( (output.first != "yaml") && (output.first.find("PostGIS") == std::string::npos) )  {
        //-- check paths of the output file
        boost::filesystem::path p(outputs[output.first]);
        try {
          boost::filesystem::path pcan = canonical(p.parent_path(), boost::filesystem::current_path());
        }
        catch (boost::filesystem::filesystem_error &e)
        {
          std::cerr << "ERROR: " << e.what() << ". Abort." << std::endl;
          return 0;
        }
      }
    }
  } 
  catch(std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }  

  std::clog << licensewarning << std::endl;


  //-- allowed feature classes
  std::set<std::string> allowedFeatures{"Building", "Water", "Terrain", "Road", "Forest", "Separation", "Bridge/Overpass"};

  //-- validate the YAML file right now, nicer for the user
  std::clog << "SKIPPING VALIDATION YAML" << std::endl;
  // TODO : write the validation of yaml updated
  // if (validate_yaml(argv[1], allowedFeatures) == false) {
  //   std::cerr << "ERROR: config file (*.yml) is not valid. Aborting.\n";
  //   return 0;
  // }
  // std::clog << "Config file is valid.\n";

  Map3d map3d;
  YAML::Node nodes = YAML::LoadFile(f_yaml);
  //-- store the lifting options in the Map3d
  YAML::Node n = nodes["lifting_options"];
  if (n["Building"]) {
    if (n["Building"]["roof"]) {
      if (n["Building"]["roof"]["height"]) {
        std::string height = n["Building"]["roof"]["height"].as<std::string>();
        map3d.set_building_heightref_roof(std::stof(height.substr(height.find_first_of("-") + 1)) / 100);
      }
      YAML::Node tmp = n["Building"]["roof"]["use_LAS_classes"];
      for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
        map3d.add_allowed_las_class(LAS_BUILDING_ROOF, it2->as<int>());
    }
    if (n["Building"]["ground"]) {
      if (n["Building"]["ground"]["height"]) {
        std::string height = n["Building"]["ground"]["height"].as<std::string>();
        map3d.set_building_heightref_ground(std::stof(height.substr(height.find_first_of("-") + 1)) / 100);
      }
      YAML::Node tmp = n["Building"]["ground"]["use_LAS_classes"];
      for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
        map3d.add_allowed_las_class(LAS_BUILDING_GROUND, it2->as<int>());
    }    
    if (n["Building"]["lod"]) {
      map3d.set_building_lod(n["Building"]["lod"].as<int>());
    }
    if (n["Building"]["triangulate"]) {
      if (n["Building"]["triangulate"].as<std::string>() == "true")
        map3d.set_building_triangulate(true);
      else
        map3d.set_building_triangulate(false);
    }
  }
  if (n["Terrain"]) {
    if (n["Terrain"]["simplification"])
      map3d.set_terrain_simplification(n["Terrain"]["simplification"].as<int>());
    if (n["Terrain"]["simplification_tinsimp"] != 0)
      map3d.set_terrain_simplification_tinsimp(n["Terrain"]["simplification_tinsimp"].as<double>());
    if (n["Terrain"]["innerbuffer"])
      map3d.set_terrain_innerbuffer(n["Terrain"]["innerbuffer"].as<float>());
    YAML::Node tmp = n["Terrain"]["use_LAS_classes"];
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
      map3d.add_allowed_las_class(LAS_TERRAIN, it2->as<int>());
  }
  if (n["Forest"]) {
    if (n["Forest"]["simplification"])
      map3d.set_forest_simplification(n["Forest"]["simplification"].as<int>());
    if (n["Forest"]["simplification_tinsimp"] != 0)
      map3d.set_forest_simplification_tinsimp(n["Forest"]["simplification_tinsimp"].as<double>());
    if (n["Forest"]["innerbuffer"])
      map3d.set_forest_innerbuffer(n["Forest"]["innerbuffer"].as<float>());
    if (n["Forest"]["ground_points_only"] && n["Forest"]["ground_points_only"].as<std::string>() == "true")
      map3d.set_forest_ground_points_only(true);
    YAML::Node tmp = n["Forest"]["use_LAS_classes"];
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
      map3d.add_allowed_las_class(LAS_FOREST, it2->as<int>());
  }
  if (n["Water"]) {
    if (n["Water"]["height"]) {
      std::string height = n["Water"]["height"].as<std::string>();
      map3d.set_water_heightref(std::stof(height.substr(height.find_first_of("-") + 1)) / 100);
    }
    YAML::Node tmp = n["Water"]["use_LAS_classes"];
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
      map3d.add_allowed_las_class(LAS_WATER, it2->as<int>());
  }
  if (n["Road"]) {
    if (n["Road"]["height"]) {
      std::string height = n["Road"]["height"].as<std::string>();
      map3d.set_road_heightref(std::stof(height.substr(height.find_first_of("-") + 1)) / 100);
    }
    YAML::Node tmp = n["Road"]["use_LAS_classes"];
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
      map3d.add_allowed_las_class(LAS_ROAD, it2->as<int>());
  }
  if (n["Separation"]) {
    if (n["Separation"]["height"]) {
      std::string height = n["Separation"]["height"].as<std::string>();
      map3d.set_separation_heightref(std::stof(height.substr(height.find_first_of("-") + 1)) / 100);
    }
    YAML::Node tmp = n["Separation"]["use_LAS_classes"];
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
      map3d.add_allowed_las_class(LAS_SEPARATION, it2->as<int>());
  }
  if (n["Bridge/Overpass"]) {
    if (n["Bridge/Overpass"]["height"]) {
      std::string height = n["Bridge/Overpass"]["height"].as<std::string>();
      map3d.set_bridge_heightref(std::stof(height.substr(height.find_first_of("-") + 1)) / 100);
    }
    YAML::Node tmp = n["Bridge/Overpass"]["use_LAS_classes"];
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
      map3d.add_allowed_las_class(LAS_BRIDGE, it2->as<int>());
  }

  n = nodes["options"];
  bool bStitching = true;
  if (n["radius_vertex_elevation"])
    map3d.set_radius_vertex_elevation(n["radius_vertex_elevation"].as<float>());
  if (n["building_radius_vertex_elevation"])
    map3d.set_building_radius_vertex_elevation(n["building_radius_vertex_elevation"].as<float>());
  if (n["threshold_jump_edges"])
    map3d.set_threshold_jump_edges(n["threshold_jump_edges"].as<float>());
  if (n["use_vertical_walls"] && n["use_vertical_walls"].as<std::string>() == "true")
    map3d.set_use_vertical_walls(true);
  if (n["stitching"]) {
    if (n["stitching"].as<std::string>() == "false")
      bStitching = false;
  }
  if (n["extent"]) {
    std::vector<std::string> extent_split = stringsplit(n["extent"].as<std::string>(), ',');
    double xmin, xmax, ymin, ymax;
    bool wentgood = true;
    try {
      (n["radius_vertex_elevation"].as<std::string>());
      xmin = boost::lexical_cast<double>(extent_split[0]);
      ymin = boost::lexical_cast<double>(extent_split[1]);
      xmax = boost::lexical_cast<double>(extent_split[2]);
      ymax = boost::lexical_cast<double>(extent_split[3]);
    }
    catch (boost::bad_lexical_cast& e) {
      wentgood = false;
    }

    if (!wentgood || xmin > xmax || ymin > ymax || boost::geometry::area(Box2(Point2(xmin, ymin), Point2(xmax, ymax))) <= 0.0) {
      std::cerr << "ERROR: The supplied extent is not valid: (" << n["extent"].as<std::string>() << "), using all polygons\n";
    }
    else
    {
      std::clog << "Using extent for polygons: (" << n["extent"].as<std::string>() << ")\n";
      map3d.set_requested_extent(xmin, ymin, xmax, ymax);
    }
  }

  //-- add the polygons to the map3d
  std::vector<PolygonFile> polygonFiles;
  n = nodes["input_polygons"];
  for (auto it = n.begin(); it != n.end(); ++it) {
    // Get the correct uniqueid attribute
    std::string uniqueid = "fid";
    if ((*it)["uniqueid"]) {
      uniqueid = (*it)["uniqueid"].as<std::string>();
    }
    // Get the correct height attribute
    std::string heightfield = "heightfield";
    if ((*it)["height_field"]) {
      heightfield = (*it)["height_field"].as<std::string>();
    }
    // Get the handle_multiple_heights setting
    bool handle_multiple_heights = false;
    if ((*it)["handle_multiple_heights"] && (*it)["handle_multiple_heights"].as<std::string>() == "true") {
      handle_multiple_heights = true;
    }

    // Get all datasets
    YAML::Node datasets = (*it)["datasets"];
    for (auto it2 = datasets.begin(); it2 != datasets.end(); ++it2) {
      PolygonFile file;
      file.filename = it2->as<std::string>();
      file.idfield = uniqueid;
      file.heightfield = heightfield;
      file.handle_multiple_heights = handle_multiple_heights;
      if ((*it)["lifting"]) {
        file.layers.emplace_back(std::string(), (*it)["lifting"].as<std::string>());
        polygonFiles.push_back(file);
      }
      else if ((*it)["lifting_per_layer"]) {
        YAML::Node layers = (*it)["lifting_per_layer"];
        for (auto it3 = layers.begin(); it3 != layers.end(); ++it3) {
          file.layers.emplace_back(it3->first.as<std::string>(), it3->second.as<std::string>());
        }
        polygonFiles.push_back(file);
      }
    }
  }

  bool added = map3d.add_polygons_files(polygonFiles);
  if (!added) {
    std::cerr << "ERROR: Missing polygon data, cannot 3dfy the dataset. Aborting.\n";
    return 0;
  }
  std::clog << "\nTotal # of polygons: " << boost::locale::as::number << map3d.get_num_polygons() << std::endl;


  map3d.save_building_variables();
  //-- spatially index the polygons
  map3d.construct_rtree();

  //-- print bbox from _rtree
  Box2 b = map3d.get_bbox();
  std::clog << std::setprecision(3) << std::fixed;
  std::clog << "Spatial extent: ("
    << bg::get<bg::min_corner, 0>(b) << ", "
    << bg::get<bg::min_corner, 1>(b) << ") ("
    << bg::get<bg::max_corner, 0>(b) << ", "
    << bg::get<bg::max_corner, 1>(b) << ")\n";
  
  int threadPoolSize = 4;
  std::vector<PointFile> fileList;

  //-- add elevation datasets
  n = nodes["input_elevation"];
  bool bElevData = false;
  for (auto it = n.begin(); it != n.end(); ++it) {
    YAML::Node tmp = (*it)["omit_LAS_classes"];
    std::vector<int> lasomits;
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2)
      lasomits.push_back(it2->as<int>());
    tmp = (*it)["datasets"];
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2) {
      int thinning = 1;
      if ((*it)["thinning"]) {
        thinning = (*it)["thinning"].as<int>();
        if (thinning == 0) {
          thinning = 1;
        }
      }

      //-- iterate over all files in directory
      boost::filesystem::path path(it2->as<std::string>());
      boost::filesystem::path rootPath = path.parent_path();
      if (path.stem() == "*") {
        if (!boost::filesystem::exists(rootPath) || !boost::filesystem::is_directory(rootPath)) {
          std::cerr << "\tERROR: " << rootPath << "is not a directory, skipping it.\n";
          bElevData = false;
          break;
        }
        else {
          boost::filesystem::recursive_directory_iterator it_end;
          for (boost::filesystem::recursive_directory_iterator it(rootPath); it != it_end; ++it) {
            if (boost::filesystem::is_regular_file(*it) && it->path().extension() == path.extension()) {
              PointFile pointFile;
              pointFile.filename = it->path().string();
              pointFile.lasomits = lasomits;
              pointFile.thinning = thinning;
              fileList.push_back(pointFile);
            }
          }
        }
      }
      else {
        PointFile pointFile;
        pointFile.filename = path.string();
        pointFile.lasomits = lasomits;
        pointFile.thinning = thinning;
        fileList.push_back(pointFile);
      }
    }
  }
  auto startPoints = boost::chrono::high_resolution_clock::now();

  for (auto file : fileList) {
    bool added = map3d.add_las_file(file);
    if (!added) {
      bElevData = false;
      break;
    }
    else {
      bElevData = true;
    }
  }

  if (bElevData == false) {
    std::cerr << "ERROR: Missing elevation data, cannot 3dfy the dataset. Aborting.\n";
    return 0;
  }

  auto durationPoints = boost::chrono::high_resolution_clock::now() - startPoints;
  printf("All points read in %lld seconds || %02d:%02d:%02d\n",
    boost::chrono::duration_cast<boost::chrono::seconds>(durationPoints).count(),
    boost::chrono::duration_cast<boost::chrono::hours>(durationPoints).count(),
    boost::chrono::duration_cast<boost::chrono::minutes>(durationPoints).count() % 60,
    (int)boost::chrono::duration_cast<boost::chrono::seconds>(durationPoints).count() % 60
  );

  std::clog << "3dfying all input polygons...\n";
  if (outputs.size() == 1) {
    if (outputs.count("CSV-BUILDINGS") == 1)
      map3d.threeDfy(false);
    else if (outputs.count("OBJ-BUILDINGS") == 1) {
      map3d.threeDfy(false);
      map3d.construct_CDT();
    }
    else if (outputs.count("CSV-BUILDINGS-MULTIPLE") == 1)
      std::clog << "CSV-BUILDINGS-MULTIPLE: no 3D reconstruction" << std::endl;
    else if (outputs.count("CSV-BUILDINGS-ALL-Z") == 1)
      std::clog << "CSV-BUILDINGS-ALL-Z: no 3D reconstruction" << std::endl;
    else {
      map3d.threeDfy(bStitching);
      map3d.construct_CDT();
    }
  }
  else {
    map3d.threeDfy(bStitching);
    map3d.construct_CDT();
  }
  std::clog << "...3dfying done.\n";

  //-- output
  n = nodes["output"];
  if (n["building_floor"].as<std::string>() == "true")
    map3d.set_building_include_floor(true);
  int z_exaggeration = 0;
  if (n["vertical_exaggeration"])
    z_exaggeration = n["vertical_exaggeration"].as<int>();

  //-- iterate over all output
  for (auto& output : outputs) {
    std::clock_t startFileWriting = std::clock(); 
    std::string format = output.first;
    if (output.second == "")
      continue;  
    
    bool fileWritten = true;
    std::ofstream of;
    std::string ofname = output.second;
    if (format != "CityJSON" && format != "Shapefile" && format != "CityGML-Multifile" && format != "CityGML-IMGeo-Multifile" && format != "PostGIS") {
      // boost::filesystem::path pcan = canonical(p, boost::filesystem::current_path());
      // boost::filesystem::path p(ofname);
      of.open(ofname);
    }
    if (format == "CityGML") {
      std::clog << "CityGML output: " << ofname << std::endl;
      map3d.get_citygml(of);
    }
    else if (format == "CityGML-Multifile") {
      std::clog << "CityGML-Multifile output: " << ofname << std::endl;
      map3d.get_citygml_multifile(ofname);
    }
    else if (format == "CityGML-IMGeo") {
      std::clog << "CityGML-IMGeo output: " << ofname << std::endl;
      map3d.get_citygml_imgeo(of);
    }
    else if (format == "CityGML-IMGeo-Multifile") {
      std::clog << "CityGML-IMGeo-Multifile output: " << ofname << std::endl;
      map3d.get_citygml_imgeo_multifile(ofname);
    }
    else if (format == "CityJSON") {
      std::clog << "CityJSON output: " << ofname << std::endl;
      fileWritten = map3d.get_cityjson(ofname);
    }  
    else if (format == "OBJ") {
      std::clog << "OBJ output: " << ofname << std::endl;
      map3d.get_obj_per_feature(of, z_exaggeration);
    }
    else if (format == "OBJ-NoID") {
      std::clog << "OBJ (without IDs) output: " << ofname << std::endl;
      map3d.get_obj_per_class(of, z_exaggeration);
    }
    else if (format == "CSV-BUILDINGS") {
      std::clog << "CSV output (only of the buildings): " << ofname << std::endl;
      map3d.get_csv_buildings(of);
    }
    else if (format == "Shapefile") {
      std::clog << "Shapefile output: " << ofname << std::endl;
      fileWritten = map3d.get_gdal_output(ofname, "ESRI Shapefile", false);
    }
    else if (format == "CSV-BUILDINGS-MULTIPLE") {
      std::clog << "CSV output with multiple heights (only of the buildings): " << ofname << std::endl;
      map3d.get_csv_buildings_multiple_heights(of);
    }
    else if (format == "Shapefile-Multi") {
      std::clog << "Shapefile output: " << ofname << std::endl;
      fileWritten = map3d.get_gdal_output(ofname, "ESRI Shapefile", true);
    }
    else if (format == "CSV-BUILDINGS-ALL-Z") {
      std::clog << "CSV output with all z values (only of the buildings): " << ofname << std::endl;
      map3d.get_csv_buildings_all_elevation_points(of);
    }
    else if (format == "PostGIS") {
      std::clog << "PostGIS output\n";
      fileWritten = map3d.get_gdal_output(ofname, "PostgreSQL", false);
    }
    else if (format == "PostGIS-Multi") {
      std::clog << "PostGIS-Multi output\n";
      fileWritten = map3d.get_gdal_output(ofname, "PostgreSQL", true);
    }
    else if (format == "PostGIS-PDOK") {
      std::clog << "PostGIS-PDOK output\n";
      fileWritten = map3d.get_pdok_output(ofname);
    }
    else if (format == "GDAL") { //-- TODO: what is this? a path? how to use?
      std::string driver = n["gdal_driver"].as<std::string>();
      std::clog << "GDAL output using driver '" + driver + "'\n";
      fileWritten = map3d.get_gdal_output(ofname, driver, false);
    }
    of.close();

    if (fileWritten) {
      printf("Features written in %ld ms\n", std::clock() - startFileWriting);
    }
    else {
      std::cerr << "ERROR: Writing features failed. Aborting.\n";
      // return 0;
    }
  }

  //-- bye-bye
  auto duration = boost::chrono::high_resolution_clock::now() - startTime;
  printf("Successfully terminated in %lld seconds || %02d:%02d:%02d\n",
    boost::chrono::duration_cast<boost::chrono::seconds>(duration).count(),
    boost::chrono::duration_cast<boost::chrono::hours>(duration).count(),
    boost::chrono::duration_cast<boost::chrono::minutes>(duration).count() % 60,
    (int)boost::chrono::duration_cast<boost::chrono::seconds>(duration).count() % 60
  );
  return 1;
}

std::string print_license() {
  std::string thelicense =
    "======================================================================\n"
    "\n3dfier: takes 2D GIS datasets and '3dfies' to create 3D city models.\n\n"
    "Copyright (C) 2015-2018  3D geoinformation research group, TU Delft\n\n"
    "3dfier is free software: you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation, either version 3 of the License, or\n"
    "(at your option) any later version.\n\n"
    "3dfier is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public License for more details.\n\n"
    "A copy of the GNU General Public License is available at\n"
    "<http://www.gnu.org/licenses/> or\n"
    "<https://github.com/tudelft3d/3dfier/blob/master/LICENSE\n\n"
    "For any information or further details about 3dfier, contact:\n"
    "Hugo Ledoux \n"
    "<h.ledoux@tudelft.nl>\n"
    "Faculty of Architecture & the Built Environment\n"
    "Delft University of Technology\n"
    "Julianalaan 134, Delft 2628BL, the Netherlands\n"
    "======================================================================";
  return thelicense;
}

bool validate_yaml(const char* arg, std::set<std::string>& allowedFeatures) {
  YAML::Node nodes;
  try {
     nodes = YAML::LoadFile(arg);
  }
  catch(const std::exception&) {
    std::cerr << "ERROR: YAML structure of config is invalid.\n";
    return false;
  }
  bool wentgood = true;
  //-- 1. input polygons classes
  YAML::Node n = nodes["input_polygons"];
  for (auto it = n.begin(); it != n.end(); ++it) {
    if ((*it)["lifting_per_layer"]) {
      YAML::Node tmp = (*it)["lifting_per_layer"];
      for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2) {
        if (allowedFeatures.count((it2->second).as<std::string>()) == 0) {
          std::cerr << "\tLifting class '" << (it2->second).as<std::string>() << "' unknown.\n";
          wentgood = false;
        }
      }
    }
    else if ((*it)["lifting"]) {
      if ((*it)["lifting"].IsNull()) {
        std::cerr << "Option 'lifting' invalid; supplied empty attribute. \n";
        wentgood = false;
      }
      else if (allowedFeatures.count((*it)["lifting"].as<std::string>()) == 0) {
        std::cerr << "\tLifting class '" << (*it)["lifting"].as<std::string>() << "' unknown.\n";
        wentgood = false;
      }
      if ((*it)["uniqueid"].IsNull()) {
        std::cerr << "Option 'uniqueid' invalid; supplied empty attribute. \n";
        wentgood = false;
      }
      if ((*it)["height_field"].IsNull()) {
        std::cerr << "Option 'height_field' invalid; supplied empty attribute. \n";
        wentgood = false;
      }
    }
  }
  //-- 2. lifting_options
  n = nodes["lifting_options"];
  if (n["Building"]) {
    if (n["Building"]["height_roof"]) {
      std::string s = n["Building"]["height_roof"].as<std::string>();
      if ((s.substr(0, s.find_first_of("-")) != "percentile") ||
        (is_string_integer(s.substr(s.find_first_of("-") + 1), 0, 100) == false)) {
        wentgood = false;
        std::cerr << "\tOption 'Building.height_roof' invalid; must be 'percentile-XX'.\n";
      }
    }
    if (n["Building"]["height_ground"]) {
      std::string s = n["Building"]["height_ground"].as<std::string>();
      if ((s.substr(0, s.find_first_of("-")) != "percentile") ||
        (is_string_integer(s.substr(s.find_first_of("-") + 1), 0, 100) == false)) {
        wentgood = false;
        std::cerr << "\tOption 'Building.height_ground' invalid; must be 'percentile-XX'.\n";
      }
    }
    if (n["Building"]["lod"]) {
      if (is_string_integer(n["Building"]["lod"].as<std::string>(), 0, 1) == false) {
        wentgood = false;
        std::cerr << "\tOption 'Building.lod' invalid; must be an integer between 0 and 1.\n";
      }
    }
    if (n["Building"]["triangulate"]) {
      std::string s = n["Building"]["triangulate"].as<std::string>();
      if ((s != "true") && (s != "false")) {
        wentgood = false;
        std::cerr << "\tOption 'Building.triangulate' invalid; must be 'true' or 'false'.\n";
      }
    }
  }
  if (n["Water"]) {
    if (n["Water"]["height"]) {
      std::string s = n["Water"]["height"].as<std::string>();
      if ((s.substr(0, s.find_first_of("-")) != "percentile") ||
        (is_string_integer(s.substr(s.find_first_of("-") + 1), 0, 100) == false)) {
        wentgood = false;
        std::cerr << "\tOption 'Water.height' invalid; must be 'percentile-XX'.\n";
      }
    }
  }
  if (n["Road"]) {
    if (n["Road"]["height"]) {
      std::string s = n["Road"]["height"].as<std::string>();
      if ((s.substr(0, s.find_first_of("-")) != "percentile") ||
        (is_string_integer(s.substr(s.find_first_of("-") + 1), 0, 100) == false)) {
        wentgood = false;
        std::cerr << "\tOption 'Road.height' invalid; must be 'percentile-XX'.\n";
      }
    }
  }
  if (n["Terrain"]) {
    if (n["Terrain"]["simplification"]) {
      if (is_string_integer(n["Terrain"]["simplification"].as<std::string>()) == false) {
        wentgood = false;
        std::cerr << "\tOption 'Terrain.simplification' invalid; must be an integer.\n";
      }
    }
    if (n["Terrain"]["simplification_tinsimp"]) {
      try {
        boost::lexical_cast<float>(n["Terrain"]["simplification_tinsimp"].as<std::string>());
      }
      catch (boost::bad_lexical_cast& e) {
        wentgood = false;
        std::cerr << "\tOption 'Terrain.simplification_tinsimp' invalid; must be a double.\n";
      }
    }
    if (n["Terrain"]["innerbuffer"]) {
      try {
        boost::lexical_cast<float>(n["Terrain"]["innerbuffer"].as<std::string>());
      }
      catch (boost::bad_lexical_cast& e) {
        wentgood = false;
        std::cerr << "\tOption 'Terrain.innerbuffer' invalid; must be a float.\n";
      }
    }
  }
  if (n["Forest"]) {
    if (n["Forest"]["simplification"]) {
      if (is_string_integer(n["Forest"]["simplification"].as<std::string>()) == false) {
        wentgood = false;
        std::cerr << "\tOption 'Forest.simplification' invalid; must be an integer.\n";
      }
    }
    if (n["Forest"]["simplification_tinsimp"]) {
      try {
        boost::lexical_cast<float>(n["Forest"]["simplification_tinsimp"].as<std::string>());
      }
      catch (boost::bad_lexical_cast& e) {
        wentgood = false;
        std::cerr << "\tOption 'Forest.simplification_tinsimp' invalid; must be a double.\n";
      }
    }
    if (n["Forest"]["innerbuffer"]) {
      try {
        boost::lexical_cast<float>(n["Forest"]["innerbuffer"].as<std::string>());
      }
      catch (boost::bad_lexical_cast& e) {
        wentgood = false;
        std::cerr << "\tOption 'Forest.innerbuffer' invalid; must be a float.\n";
      }
    }
  }
  if (n["Separation"]) {
    if (n["Separation"]["height"]) {
      std::string s = n["Separation"]["height"].as<std::string>();
      if ((s.substr(0, s.find_first_of("-")) != "percentile") ||
        (is_string_integer(s.substr(s.find_first_of("-") + 1), 0, 100) == false)) {
        wentgood = false;
        std::cerr << "\tOption 'Separation.height' invalid; must be 'percentile-XX'.\n";
      }
    }
  }
  if (n["Bridge/Overpass"]) {
    if (n["Bridge/Overpass"]["height"]) {
      std::string s = n["Bridge/Overpass"]["height"].as<std::string>();
      if ((s.substr(0, s.find_first_of("-")) != "percentile") ||
        (is_string_integer(s.substr(s.find_first_of("-") + 1), 0, 100) == false)) {
        wentgood = false;
        std::cerr << "\tOption 'Bridge/Overpass.height' invalid; must be 'percentile-XX'.\n";
      }
    }
  }
  //-- 3. input_elevation
  n = nodes["input_elevation"];
  for (auto it = n.begin(); it != n.end(); ++it) {
    YAML::Node tmp = (*it)["omit_LAS_classes"];
    for (auto it2 = tmp.begin(); it2 != tmp.end(); ++it2) {
      if (is_string_integer(it2->as<std::string>()) == false) {
        wentgood = false;
        std::cerr << "\tOption 'input_elevation.omit_LAS_class' invalid; must be an integer.\n";
      }
    }
    if ((*it)["thinning"]) {
      if (is_string_integer((*it)["thinning"].as<std::string>()) == false) {
        wentgood = false;
        std::cerr << "\tOption 'input_elevation.thinning' invalid; must be an integer.\n";
      }
    }
  }
  //-- 4. options
  n = nodes["options"];
  if (n["radius_vertex_elevation"]) {
    try {
      boost::lexical_cast<float>(n["radius_vertex_elevation"].as<std::string>());
    }
    catch (boost::bad_lexical_cast& e) {
      wentgood = false;
      std::cerr << "\tOption 'options.radius_vertex_elevation' invalid.\n";
    }
  }
  if (n["threshold_jump_edges"]) {
    try {
      boost::lexical_cast<float>(n["threshold_jump_edges"].as<std::string>());
    }
    catch (boost::bad_lexical_cast& e) {
      wentgood = false;
      std::cerr << "\tOption 'options.threshold_jump_edges' invalid.\n";
    }
  }
  //-- 5. output
  n = nodes["output"];
  std::string format = n["format"].as<std::string>();
  if ((format != "OBJ") &&
    (format != "OBJ-NoID") &&
    (format != "CityJSON") &&
    (format != "CityGML") &&
    (format != "CityGML-Multifile") &&
    (format != "CityGML-IMGeo") &&
    (format != "CityGML-IMGeo-Multifile") &&
    (format != "OBJ-BUILDINGS") &&
    (format != "CSV-BUILDINGS") &&
    (format != "Shapefile") &&
    (format != "Shapefile-Multi") &&
    (format != "CSV-BUILDINGS-MULTIPLE") &&
    (format != "CSV-BUILDINGS-ALL-Z") &&
    (format != "PostGIS") &&
    (format != "PostGIS-Multi") &&
    (format != "PostGIS-PDOK") &&
    (format != "GDAL")) {
    wentgood = false;
    std::cerr << "\tOption 'output.format' invalid (OBJ | OBJ-NoID | CityGML | CityGML-Multifile | CityGML-IMGeo | CityGML-IMGeo-Multifile | CSV-BUILDINGS | Shapefile | Shapefile-Multi | PostGIS | PostGIS-Multi | PostGIS-PDOK)\n";
  }
  if (format == "GDAL" && (!n["gdal_driver"] || n["gdal_driver"].as<std::string>().empty())) {
    wentgood = false;
    std::cerr << "\tOption 'output.format' GDAL needs gdal_driver setting\n";
  }

  return wentgood;
}

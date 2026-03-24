// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/geojson.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ftxui::ui {

// ── Minimal JSON value tree ───────────────────────────────────────────────────

namespace {

struct JVal {
  enum class T { Null, Bool, Num, Str, Arr, Obj } tag = T::Null;
  bool b = false;
  double n = 0.0;
  std::string s;
  std::vector<JVal> a;
  std::map<std::string, JVal> o;

  bool has(const std::string& key) const {
    return tag == T::Obj && o.count(key) > 0;
  }
  const JVal& get(const std::string& key) const {
    static const JVal kNull;
    auto it = o.find(key);
    return it != o.end() ? it->second : kNull;
  }
};

// ── Recursive descent parser ──────────────────────────────────────────────────

struct JParse {
  const char* p;
  const char* end;

  explicit JParse(std::string_view sv)
      : p(sv.data()), end(sv.data() + sv.size()) {}

  void ws() {
    while (p < end &&
           (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
      ++p;
    }
  }

  JVal parse() {
    ws();
    if (p >= end) return {};
    switch (*p) {
      case '{': return obj();
      case '[': return arr();
      case '"': return str();
      case 'n':
        p += 4;
        return {};
      case 't': {
        p += 4;
        JVal v;
        v.tag = JVal::T::Bool;
        v.b = true;
        return v;
      }
      case 'f': {
        p += 5;
        JVal v;
        v.tag = JVal::T::Bool;
        v.b = false;
        return v;
      }
      default:
        return num();
    }
  }

  JVal obj() {
    JVal v;
    v.tag = JVal::T::Obj;
    ++p;  // '{'
    ws();
    while (p < end && *p != '}') {
      if (*p == ',') {
        ++p;
        ws();
        continue;
      }
      if (*p != '"') {
        ++p;
        continue;
      }
      std::string key = str().s;
      ws();
      if (p < end && *p == ':') ++p;
      ws();
      v.o[key] = parse();
      ws();
    }
    if (p < end) ++p;  // '}'
    return v;
  }

  JVal arr() {
    JVal v;
    v.tag = JVal::T::Arr;
    ++p;  // '['
    ws();
    while (p < end && *p != ']') {
      if (*p == ',') {
        ++p;
        ws();
        continue;
      }
      v.a.push_back(parse());
      ws();
    }
    if (p < end) ++p;  // ']'
    return v;
  }

  JVal str() {
    JVal v;
    v.tag = JVal::T::Str;
    ++p;  // '"'
    while (p < end && *p != '"') {
      if (*p == '\\') {
        ++p;
        if (p < end) {
          char c = *p++;
          switch (c) {
            case '"':  v.s += '"';  break;
            case '\\': v.s += '\\'; break;
            case '/':  v.s += '/';  break;
            case 'n':  v.s += '\n'; break;
            case 'r':  v.s += '\r'; break;
            case 't':  v.s += '\t'; break;
            case 'u':
              // skip 4 hex digits
              for (int i = 0; i < 4 && p < end; ++i, ++p) {}
              v.s += '?';
              break;
            default:   v.s += c;    break;
          }
        }
      } else {
        v.s += *p++;
      }
    }
    if (p < end) ++p;  // '"'
    return v;
  }

  JVal num() {
    JVal v;
    v.tag = JVal::T::Num;
    const char* start = p;
    if (p < end && (*p == '-' || *p == '+')) ++p;
    while (p < end &&
           ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' ||
            *p == 'E' || *p == '-' || *p == '+')) {
      ++p;
    }
    if (p > start) {
      try {
        v.n = std::stod(std::string(start, p));
      } catch (...) {
        v.n = 0.0;
      }
    }
    return v;
  }
};

// ── GeoJSON coordinate extraction ─────────────────────────────────────────────

GeoPoint ToPoint(const JVal& v) {
  if (v.tag != JVal::T::Arr || v.a.size() < 2) return {};
  return {v.a[0].n, v.a[1].n};
}

GeoRing ToRing(const JVal& v) {
  GeoRing ring;
  if (v.tag != JVal::T::Arr) return ring;
  ring.reserve(v.a.size());
  for (const auto& pt : v.a) {
    ring.push_back(ToPoint(pt));
  }
  return ring;
}

void FillCoordinates(GeoGeometry& geom, const JVal& coords) {
  const std::string& type = geom.type;
  if (type == "Point") {
    geom.points.push_back(ToPoint(coords));
  } else if (type == "MultiPoint") {
    for (const auto& pt : coords.a) {
      geom.points.push_back(ToPoint(pt));
    }
  } else if (type == "LineString") {
    for (const auto& pt : coords.a) {
      geom.points.push_back(ToPoint(pt));
    }
  } else if (type == "MultiLineString") {
    for (const auto& line : coords.a) {
      for (const auto& pt : line.a) {
        geom.points.push_back(ToPoint(pt));
      }
    }
  } else if (type == "Polygon") {
    for (const auto& ring_val : coords.a) {
      geom.rings.push_back(ToRing(ring_val));
    }
  } else if (type == "MultiPolygon") {
    for (const auto& poly_val : coords.a) {
      std::vector<GeoRing> poly;
      poly.reserve(poly_val.a.size());
      for (const auto& ring_val : poly_val.a) {
        poly.push_back(ToRing(ring_val));
      }
      geom.multipolygon.push_back(std::move(poly));
    }
  }
}

}  // namespace

// ── Public API ────────────────────────────────────────────────────────────────

std::string GeoFeature::name() const {
  auto it = properties.find("name");
  return it != properties.end() ? it->second : "";
}

void GeoCollection::ComputeBounds() {
  min_lon = 180;
  max_lon = -180;
  min_lat = 90;
  max_lat = -90;

  auto update = [&](const GeoPoint& p) {
    if (p.lon < min_lon) min_lon = p.lon;
    if (p.lon > max_lon) max_lon = p.lon;
    if (p.lat < min_lat) min_lat = p.lat;
    if (p.lat > max_lat) max_lat = p.lat;
  };

  for (const auto& feat : features) {
    for (const auto& pt : feat.geometry.points) update(pt);
    for (const auto& ring : feat.geometry.rings) {
      for (const auto& pt : ring) update(pt);
    }
    for (const auto& poly : feat.geometry.multipolygon) {
      for (const auto& ring : poly) {
        for (const auto& pt : ring) update(pt);
      }
    }
  }

  if (min_lon > max_lon) {
    min_lon = -180;
    max_lon = 180;
    min_lat = -90;
    max_lat = 90;
  }
}

GeoCollection ParseGeoJSON(std::string_view json) {
  GeoCollection col;
  JParse parser(json);
  JVal root = parser.parse();

  if (root.tag != JVal::T::Obj) return col;

  const JVal& features_val = root.get("features");
  if (features_val.tag != JVal::T::Arr) return col;

  for (const auto& fv : features_val.a) {
    if (fv.tag != JVal::T::Obj) continue;

    GeoFeature feature;

    // Extract properties
    const JVal& props = fv.get("properties");
    if (props.tag == JVal::T::Obj) {
      for (const auto& [k, v] : props.o) {
        if (v.tag == JVal::T::Str) {
          feature.properties[k] = v.s;
        } else if (v.tag == JVal::T::Num) {
          feature.properties[k] = std::to_string(v.n);
        } else if (v.tag == JVal::T::Bool) {
          feature.properties[k] = v.b ? "true" : "false";
        }
      }
    }

    // Extract geometry
    const JVal& geom_val = fv.get("geometry");
    if (geom_val.tag == JVal::T::Obj) {
      feature.geometry.type = geom_val.get("type").s;
      const JVal& coords = geom_val.get("coordinates");
      FillCoordinates(feature.geometry, coords);
    }

    col.features.push_back(std::move(feature));
  }

  col.ComputeBounds();
  return col;
}

GeoCollection LoadGeoJSON(std::string_view path) {
  std::string path_str(path);
  std::ifstream file(path_str);
  if (!file.is_open()) return {};
  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  return ParseGeoJSON(content);
}

}  // namespace ftxui::ui

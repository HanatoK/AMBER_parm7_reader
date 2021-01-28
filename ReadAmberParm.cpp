#include "ReadAmberParm.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <utility>
#include <iomanip>

namespace AmberParm7Reader {

void parse_fortran_format(const std::string &str,
                          FortranFormatSpecifier &specifier) {
  using std::back_inserter;
  using std::copy;
  using std::cout;
  using std::regex;
  using std::regex_search;
  using std::smatch;
  using std::sregex_token_iterator;
  using std::string;
  using std::vector;
  // check if this is a padding or a real format specifier
  size_t left_quotation_mark = str.find("'");
  if (left_quotation_mark == string::npos) {
    specifier.IsPadding = false;
    specifier.Padding.clear();
    vector<string> word_list;
    regex type_char{"[A-Za-z\\.]"};
    // split the string by [A-Za-z\.] and keep the separator
    copy(sregex_token_iterator(str.begin(), str.end(), type_char, {-1, 0}),
         sregex_token_iterator(), back_inserter(word_list));
    if (!word_list.front().empty()) {
      specifier.NumOfFields = std::stoul(word_list[0].c_str());
    } else {
      // if this is empty, then means the str starting with the separator
      // ("i10", "E6.2")
      specifier.NumOfFields = 1;
    }
    specifier.Type = word_list[1][0];
    specifier.Width = std::stoul(word_list[2]);
    if (word_list.size() == 5) {
      specifier.Precision = std::stoul(word_list[4]);
    } else {
      specifier.Precision = -1;
    }
  } else {
    size_t right_quotation_mark = str.find("'", left_quotation_mark + 1);
    specifier.IsPadding = true;
    specifier.Padding =
        str.substr(left_quotation_mark + 1,
                   right_quotation_mark - (left_quotation_mark + 1));
    specifier.NumOfFields = 1;
  }
}

void split_string(const std::string& data,
                  const std::string& delim,
                  std::vector<std::string>& dest) {
  size_t index = 0, new_index = 0;
  std::string tmpstr;
  while (index != data.length()) {
    new_index = data.find(delim, index);
    if (new_index != std::string::npos) tmpstr = data.substr(index, new_index - index);
    else tmpstr = data.substr(index, data.length());
    if (!tmpstr.empty()) {
        dest.push_back(tmpstr);
    }
    if (new_index == std::string::npos) break;
    index = new_index + 1;
  }
}

void parse_fortran_format_noregex(const std::string &str,
                                  FortranFormatSpecifier &specifier) {
  size_t left_quotation_mark = str.find("'");
  if (left_quotation_mark == std::string::npos) {
    std::string num_fields, type, width, dot, precision;
    for (size_t i = 0; i < str.size(); ++i) {
      // check if this is a digit at first.
      if (std::isdigit(str[i])) {
        // then check if the digit is before or after the type.
        if (type.size() > 0) {
          // if type is already read,
          // then append this character to "width" or "precision".
          if (dot.size() > 0) {
            // the digit after "." denotes the precision.
            precision += str[i];
          } else {
            // the digit is after the type field but before ".".
            width += str[i];
          }
        } else {
          // the digit before the type represents the number of fields.
          num_fields += str[i];
        }
      } else if (std::isalpha(str[i])) {
        // if it is [A-Za-z] then it denotes the type.
        type += str[i];
      } else if (str[i] == '.') {
        dot += str[i];
      } else {
        // skip other characters
        continue;
      }
    }
    // populate the FortranFormatSpecifier struct
    if (num_fields.size() > 0) {
      specifier.NumOfFields = std::stoul(num_fields.c_str());
    } else {
      // ("i10", "E6.2")
      specifier.NumOfFields = 1;
    }
    // must have "type"
    specifier.Type = type.front();
    // must have "width"
    specifier.Width = std::stoul(width.c_str());
    if (precision.size() > 0) {
      specifier.Precision = std::stoul(precision.c_str());
    } else {
      // integers have no precision fields
      specifier.Precision = -1;
    }
  } else {
    size_t right_quotation_mark = str.find("'", left_quotation_mark + 1);
    specifier.IsPadding = true;
    specifier.Padding =
        str.substr(left_quotation_mark + 1,
                   right_quotation_mark - (left_quotation_mark + 1));
    specifier.NumOfFields = 1;
  }
}

void parse_fortran_format_noregex(const std::string &str,
                                  vector<FortranFormatSpecifier> &specifiers) {
  // split the string by comma
  vector<string> sub_format_strings;
  split_string(str, ",", sub_format_strings);
  specifiers.resize(sub_format_strings.size());
  for (size_t i = 0; i < sub_format_strings.size(); ++i) {
    parse_fortran_format_noregex(sub_format_strings[i], specifiers[i]);
  }
}

void parse_fortran_format(const std::string &str,
                          vector<FortranFormatSpecifier> &specifiers) {
  using std::back_inserter;
  using std::copy;
  using std::regex;
  using std::regex_search;
  using std::smatch;
  using std::sregex_token_iterator;
  vector<string> sub_format_strings;
  // split the string by comma
  regex comma{","};
  copy(sregex_token_iterator(str.begin(), str.end(), comma, -1),
       sregex_token_iterator(), back_inserter(sub_format_strings));
  specifiers.resize(sub_format_strings.size());
  for (size_t i = 0; i < sub_format_strings.size(); ++i) {
    parse_fortran_format(sub_format_strings[i], specifiers[i]);
  }
}

void split_string_by_specifiers(
    const string &source, const vector<FortranFormatSpecifier> &specifiers,
    vector<FortranData> &destination) {
  size_t start_pos = 0;
  for (const auto &spec : specifiers) {
    if (spec.IsPadding == true) {
      // I should check padding here but I don't know how to handle the it
      // Anyway, AMBER parm7 doesn't use padding
    } else {
      for (int i = 0; i < spec.NumOfFields; ++i) {
        if (start_pos < source.size()) {
          const string tmp = source.substr(start_pos, spec.Width);
          switch (toupper(spec.Type)) {
          case 'I':
            destination.push_back(std::stoi(tmp));
            break;
          case 'A':
            destination.push_back(tmp);
            break;
#ifdef DOUBLE
          case 'D':
            destination.push_back(std::stod(tmp));
            break;
          case 'E':
            destination.push_back(std::stod(tmp));
            break;
          case 'G':
            destination.push_back(std::stod(tmp));
            break;
          case 'F':
            destination.push_back(std::stod(tmp));
            break;
#else
          case 'D':
            destination.push_back(std::stof(tmp));
            break;
          case 'E':
            destination.push_back(std::stof(tmp));
            break;
          case 'G':
            destination.push_back(std::stof(tmp));
            break;
          case 'F':
            destination.push_back(std::stof(tmp));
            break;
#endif
          }
          start_pos += spec.Width;
        }
      }
    }
  }
}

bool read_amber_parm_stage1(const char *filename, AmberTopparMap &toppar_map) {
  using std::ifstream;
  using std::istringstream;
  using std::string;
  bool success = true;
  string error_msg;
  ifstream ifs_parm(filename);
  if (!ifs_parm.is_open()) return false;
  string line;
  string current_flag;
  bool match_flag = false;
  bool match_format = false;
  //   vector<FortranData> section_data;
  vector<FortranFormatSpecifier> specifiers;
  tuple<bool, vector<FortranData>> *p_map = nullptr;
  istringstream iss;
  while (std::getline(ifs_parm, line)) {
    /* AMBER format parser requirement 3:
     * Parsers should not expect or require %COMMENT lines to exist,
     * but should properly parse the file if any number of %COMMENT
     * lines appear as indicated above.
     */
    if (line[0] == '%') {
      if (line.find("%COMMENT") == 0) {
        continue;
      } else if (line.find("%VERSION") == 0) {
        // parse the version line, read version_stamp and build time
        char tmp_version_stamp[16];
        char tmp_date[16];
        char tmp_time[16];
        sscanf(line.c_str(), "%%VERSION VERSION_STAMP = %9s DATE = %8s %8s",
               tmp_version_stamp, tmp_date, tmp_time);
        continue;
      } else if (line.find("%FLAG") == 0) {
        // parse flags
        if (match_flag && match_format) {
          // insert the previous section into map
          current_flag.clear();
          specifiers.clear();
          match_flag = false;
          match_format = false;
        }
        istringstream iss(line.substr(strlen("%FLAG")));
        iss >> current_flag;
        p_map = &(toppar_map[current_flag]);
        std::get<0>(*p_map) = false;
        match_flag = true;
        continue;
      } else if (line.find("%FORMAT") == 0) {
        // parse format
        if (match_flag == false) {
          success = false;
          break;
        }
        const size_t format_start = line.find("(", strlen("%FORMAT"));
        const size_t format_end = line.find(")", format_start + 1);
        parse_fortran_format_noregex(
            line.substr(format_start + 1, format_end - (format_start + 1)),
            specifiers);
        match_format = true;
        continue;
      }
    }
    // parse data fields
    if (match_flag && match_format) {
      // read the according section
      split_string_by_specifiers(line, specifiers, std::get<1>(*p_map));
    }
  }
  return success;
}

bool read_amber_parm_stage2(AmberTopparMap &toppar_map,
                            Ambertoppar &toppar_data) {
  using std::cerr;
  using std::get;
  using std::ref;
  bool success = true;
  // TITLE
  {
    // check TITLE to see if this is a AMBER force field
    auto current_section = toppar_map.find("TITLE");
    if (current_section == toppar_map.end()) {
      // check if this is a CHARMM force field in AMBER format
      current_section = toppar_map.find("CTITLE");
      if (current_section == toppar_map.end()) {
        cerr << "TITLE/CTITLE section for AMBER/CHARMM force field is not "
                "found!\n";
        toppar_data.IsCharmmFF = false;
        success = false;
        cerr << "Failed to read title!\n";
      } else {
        toppar_data.IsCharmmFF = true;
      }
    } else {
      toppar_data.IsCharmmFF = false;
    }
    toppar_data.ititl = get<1>(current_section->second).size() > 0 ? get<1>(current_section->second)[0].String : string();
    get<0>(current_section->second) = true;
  }
  // POINTERS
  if (success) {
    const string section_name{"POINTERS"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_pointer(get<1>(current_section->second), toppar_data);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // ATOM_NAME
  if (success) {
    const string section_name{"ATOM_NAME"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                              toppar_data.AtomNames, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // CHARGE
  if (success) {
    const string section_name{"CHARGE"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                              toppar_data.Charges, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // ATOMIC_NUMBER
  if (success) {
    const string section_name{"ATOMIC_NUMBER"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                              toppar_data.AtomNumbers, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // MASS
  if (success) {
    const string section_name{"MASS"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                              toppar_data.Masses, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // ATOM_TYPE_INDEX
  if (success) {
    const string section_name{"ATOM_TYPE_INDEX"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                    toppar_data.Iac, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // NUMBER_EXCLUDED_ATOMS
  if (success) {
    const string section_name{"NUMBER_EXCLUDED_ATOMS"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                    toppar_data.Iblo, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // NONBONDED_PARM_INDEX
  if (success) {
    const int Ntype2d = toppar_data.Ntypes * toppar_data.Ntypes;
    const string section_name{"NONBONDED_PARM_INDEX"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), Ntype2d, toppar_data.Cno,
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // RESIDUE_LABEL
  if (success) {
    const string section_name{"RESIDUE_LABEL"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nres,
                    toppar_data.ResNames, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // RESIDUE_POINTER
  if (success) {
    const string section_name{"RESIDUE_POINTER"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nres,
                    toppar_data.Ipres, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // BOND_FORCE_CONSTANT
  if (success) {
    const string section_name{"BOND_FORCE_CONSTANT"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Numbnd,
                    toppar_data.Rk, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // BOND_EQUIL_VALUE
  if (success) {
    const string section_name{"BOND_EQUIL_VALUE"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Numbnd,
                    toppar_data.Req, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // ANGLE_FORCE_CONSTANT
  if (success) {
    const string section_name{"ANGLE_FORCE_CONSTANT"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Numang,
                    toppar_data.Tk, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // ANGLE_EQUIL_VALUE
  if (success) {
    const string section_name{"ANGLE_EQUIL_VALUE"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Numang,
                    toppar_data.Teq, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // DIHEDRAL_FORCE_CONSTANT
  if (success) {
    const string section_name{"DIHEDRAL_FORCE_CONSTANT"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nptra,
                    toppar_data.Pk, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // DIHEDRAL_PERIODICITY
  if (success) {
    const string section_name{"DIHEDRAL_PERIODICITY"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nptra,
                    toppar_data.Pn, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // DIHEDRAL_PHASE
  if (success) {
    const string section_name{"DIHEDRAL_PHASE"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nptra,
                    toppar_data.Phase, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // SCEE_SCALE_FACTOR
  if (success) {
    const string section_name{"SCEE_SCALE_FACTOR"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nptra,
                    toppar_data.SceeScaleFactor, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // SCNB_SCALE_FACTOR
  if (success) {
    const string section_name{"SCNB_SCALE_FACTOR"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nptra,
                    toppar_data.ScnbScaleFactor, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // SOLTY (UNUSED)
  if (success) {
    const string section_name{"SOLTY"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      //       success = parse_section(get<1>(current_section->second),
      //                     toppar_data.Natyp, toppar_data.Solty,
      //                     section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // LENNARD_JONES_ACOEF
  if (success) {
    const int Nttyp = toppar_data.Ntypes * (toppar_data.Ntypes + 1) / 2;
    const string section_name{"LENNARD_JONES_ACOEF"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), Nttyp, toppar_data.Cn1,
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // LENNARD_JONES_BCOEF
  if (success) {
    const int Nttyp = toppar_data.Ntypes * (toppar_data.Ntypes + 1) / 2;
    const string section_name{"LENNARD_JONES_BCOEF"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), Nttyp, toppar_data.Cn2,
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // BONDS_INC_HYDROGEN
  if (success) {
    const string section_name{"BONDS_INC_HYDROGEN"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nbonh,
                    {ref(toppar_data.BondHAt1), ref(toppar_data.BondHAt2),
                     ref(toppar_data.BondHNum)},
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // BONDS_WITHOUT_HYDROGEN
  if (success) {
    const string section_name{"BONDS_WITHOUT_HYDROGEN"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nbona,
                    {ref(toppar_data.BondAt1), ref(toppar_data.BondAt2),
                     ref(toppar_data.BondNum)},
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // ANGLES_INC_HYDROGEN
  if (success) {
    const string section_name{"ANGLES_INC_HYDROGEN"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Ntheth,
                    {ref(toppar_data.AngleHAt1), ref(toppar_data.AngleHAt2),
                     ref(toppar_data.AngleHAt3), ref(toppar_data.AngleHNum)},
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // ANGLES_WITHOUT_HYDROGEN
  if (success) {
    const string section_name{"ANGLES_WITHOUT_HYDROGEN"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Ntheta,
                    {ref(toppar_data.AngleAt1), ref(toppar_data.AngleAt2),
                     ref(toppar_data.AngleAt3), ref(toppar_data.AngleNum)},
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // DIHEDRALS_INC_HYDROGEN
  if (success) {
    const string section_name{"DIHEDRALS_INC_HYDROGEN"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nphih,
                    {ref(toppar_data.DihHAt1), ref(toppar_data.DihHAt2),
                     ref(toppar_data.DihHAt3), ref(toppar_data.DihHAt4),
                     ref(toppar_data.DihHNum)},
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // DIHEDRALS_WITHOUT_HYDROGEN
  if (success) {
    const string section_name{"DIHEDRALS_WITHOUT_HYDROGEN"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nphia,
                    {ref(toppar_data.DihAt1), ref(toppar_data.DihAt2),
                     ref(toppar_data.DihAt3), ref(toppar_data.DihAt4),
                     ref(toppar_data.DihNum)},
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // EXCLUDED_ATOMS_LIST
  if (success) {
    const string section_name{"EXCLUDED_ATOMS_LIST"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nnb,
                    toppar_data.ExclAt, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // HBOND_ACOEF
  if (success) {
    const string section_name{"HBOND_ACOEF"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nphb,
                    toppar_data.HB12, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // HBOND_BCOEF
  if (success) {
    const string section_name{"HBOND_BCOEF"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Nphb,
                    toppar_data.HB10, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // HBCUT (UNUSED)
  if (success) {
    const string section_name{"HBCUT"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      // skip HBCUT
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // AMBER_ATOM_TYPE
  if (success) {
    const string section_name{"AMBER_ATOM_TYPE"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                    toppar_data.AtomSym, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // TREE_CHAIN_CLASSIFICATION
  if (success) {
    const string section_name{"TREE_CHAIN_CLASSIFICATION"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                    toppar_data.AtomTree, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // JOIN_ARRAY (UNUSED)
  if (success) {
    const string section_name{"JOIN_ARRAY"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      //       success = parse_section(get<1>(current_section->second),
      //                     toppar_data.Natom, toppar_data.TreeJoin,
      //                     section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // IROTAT (UNUSED)
  if (success) {
    const string section_name{"IROTAT"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
//             success = parse_section(get<1>(current_section->second),
//                           toppar_data.Natom, toppar_data.IrotatArray,
//                           section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // parse periodic boundary conditions only if IFBOX > 0
  if (toppar_data.Ifbox > 0) {
    // SOLVENT_POINTERS
    if (success) {
      const string section_name{"SOLVENT_POINTERS"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        if (get<1>(current_section->second).size() == 3) {
          toppar_data.Iptres = get<1>(current_section->second)[0].Int;
          toppar_data.Nspm = get<1>(current_section->second)[1].Int;
          toppar_data.Nspsol = get<1>(current_section->second)[2].Int;
          toppar_data.AtomsPerMolecule.reserve(toppar_data.Nspm);
          toppar_data.BoxDimensions.reserve(4);
          toppar_data.CapInfo.reserve(1);
          toppar_data.CapInfo2.reserve(4);
          get<0>(current_section->second) = true;
        } else {
          std::cerr << "Expect " << 3 << " but " << get<1>(current_section->second).size() << " "
                    << section_name << " are defined\n";
          success = false;
        }
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // ATOMS_PER_MOLECULE
    if (success) {
      const string section_name{"ATOMS_PER_MOLECULE"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), toppar_data.Nspm,
                      toppar_data.AtomsPerMolecule, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // BOX_DIMENSIONS
    if (success) {
      const string section_name{"BOX_DIMENSIONS"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), 4,
                      toppar_data.BoxDimensions, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
  }
  if (toppar_data.Ifcap > 0) {
    // CAP_INFO
    if (success) {
      const string section_name{"CAP_INFO"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), 1, toppar_data.CapInfo,
                      section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CAP_INFO2
    if (success) {
      const string section_name{"CAP_INFO2"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), 4, toppar_data.CapInfo2,
                      section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
  }
  // RADIUS_SET
  if (success) {
    const string section_name{"RADIUS_SET"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), 1, toppar_data.RadiusSet,
                    section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // RADII
  if (success) {
    const string section_name{"RADII"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                    toppar_data.Radii, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // SCREEN
  if (success) {
    const string section_name{"SCREEN"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                    toppar_data.Screen, section_name);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  // parse IPOL to determine the polarizability
  if (success) {
    const string section_name{"IPOL"};
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      toppar_data.Ipol = get<1>(current_section->second)[0].Int;
      toppar_data.Polarizability.reserve(toppar_data.Natom);
      get<0>(current_section->second) = true;
    } else {
      cerr << "Missing " << section_name << " section!\n";
    }
  }
  if (toppar_data.Ipol > 0) {
    // POLARIZABILITY
    if (success) {
      const string section_name{"POLARIZABILITY"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), toppar_data.Natom,
                      toppar_data.Polarizability, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
  }
  if (toppar_data.IsCharmmFF) {
    // I don't think supporting CHARMBER is a good idea since we have supported,
    // CHARMM force field in PSF format, and the user should use PSF instead of
    // AMBER format.
    // CHARMM_UREY_BRADLEY_COUNT
    if (success) {
      const string section_name{"CHARMM_UREY_BRADLEY_COUNT"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        if (get<1>(current_section->second).size() == 2) {
          toppar_data.Nub = get<1>(current_section->second)[0].Int;
          toppar_data.Nubtypes = get<1>(current_section->second)[1].Int;
          toppar_data.UreyBradleyAt1.reserve(toppar_data.Nub);
          toppar_data.UreyBradleyAt2.reserve(toppar_data.Nub);
          toppar_data.UreyBradleyNum.reserve(toppar_data.Nub);
          toppar_data.UreyBradleyForceConstants.reserve(toppar_data.Nubtypes);
          toppar_data.UreyBradleyEquilValues.reserve(toppar_data.Nubtypes);
          get<0>(current_section->second) = true;
        } else {
          std::cerr << "Expect " << 2 << " but " << get<1>(current_section->second).size() << " "
                    << section_name << " are defined\n";
          success = false;
        }
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_UREY_BRADLEY
    if (success) {
      const string section_name{"CHARMM_UREY_BRADLEY"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), toppar_data.Nub,
                      {ref(toppar_data.UreyBradleyAt1),
                       ref(toppar_data.UreyBradleyAt2),
                       ref(toppar_data.UreyBradleyNum)},
                      section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_UREY_BRADLEY_FORCE_CONSTANT
    if (success) {
      const string section_name{"CHARMM_UREY_BRADLEY_FORCE_CONSTANT"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), toppar_data.Nubtypes,
                      toppar_data.UreyBradleyForceConstants, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_UREY_BRADLEY_EQUIL_VALUE
    if (success) {
      const string section_name{"CHARMM_UREY_BRADLEY_EQUIL_VALUE"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), toppar_data.Nubtypes,
                      toppar_data.UreyBradleyEquilValues, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_NUM_IMPROPERS
    if (success) {
      const string section_name{"CHARMM_NUM_IMPROPERS"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        if (get<1>(current_section->second).size() == 1) {
          toppar_data.Nimphi = get<1>(current_section->second)[0].Int;
          toppar_data.ImproperAt1.reserve(toppar_data.Nimphi);
          toppar_data.ImproperAt2.reserve(toppar_data.Nimphi);
          toppar_data.ImproperAt3.reserve(toppar_data.Nimphi);
          toppar_data.ImproperAt4.reserve(toppar_data.Nimphi);
          toppar_data.ImproperNum.reserve(toppar_data.Nimphi);
          get<0>(current_section->second) = true;
        } else {
          std::cerr << "Expect " << 1 << " but " << get<1>(current_section->second).size() << " "
                    << section_name << " are defined\n";
          success = false;
        }
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_IMPROPERS
    if (success) {
      const string section_name{"CHARMM_IMPROPERS"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(
            get<1>(current_section->second), toppar_data.Nimphi,
            {ref(toppar_data.ImproperAt1), ref(toppar_data.ImproperAt2),
             ref(toppar_data.ImproperAt3), ref(toppar_data.ImproperAt4),
             ref(toppar_data.ImproperNum)},
            section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_NUM_IMPR_TYPES
    // Not "CHARMM_NUM_IMPROPER_TYPES" in the documentation!
    if (success) {
      const string section_name{"CHARMM_NUM_IMPR_TYPES"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        toppar_data.NimprTypes = get<1>(current_section->second)[0].Int;
        toppar_data.ImproperForceConstants.reserve(toppar_data.NimprTypes);
        toppar_data.ImproperPhases.reserve(toppar_data.NimprTypes);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_IMPROPER_FORCE_CONSTANT
    if (success) {
      const string section_name{"CHARMM_IMPROPER_FORCE_CONSTANT"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), toppar_data.NimprTypes,
                      toppar_data.ImproperForceConstants, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_IMPROPER_PHASE
    if (success) {
      const string section_name{"CHARMM_IMPROPER_PHASE"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second), toppar_data.NimprTypes,
                      toppar_data.ImproperPhases, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // LENNARD_JONES_14_ACOEF
    if (success) {
      const int Nttyp = toppar_data.Ntypes * (toppar_data.Ntypes + 1) / 2;
      const string section_name{"LENNARD_JONES_14_ACOEF"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        // this is not assigned before
        toppar_data.LJ14ACoefficients.reserve(Nttyp);
        success = parse_section(get<1>(current_section->second), Nttyp,
                      toppar_data.LJ14ACoefficients, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // LENNARD_JONES_14_BCOEF
    if (success) {
      const int Nttyp = toppar_data.Ntypes * (toppar_data.Ntypes + 1) / 2;
      const string section_name{"LENNARD_JONES_14_BCOEF"};
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        // this is not assigned before
        toppar_data.LJ14BCoefficients.reserve(Nttyp);
        success = parse_section(get<1>(current_section->second), Nttyp,
                      toppar_data.LJ14BCoefficients, section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
  }
  // Trying to parse CMAP terms
  // ff19SB also have CMAP terms, so CMAP is not CHARMM-specific.
  // However, in CHARMM FF they are prefixed with CHARMM_
  // CMAP_COUNT or CHARMM_CMAP_COUNT
  if (success) {
    const string section_name = toppar_data.IsCharmmFF
                                    ? string("CHARMM_CMAP_COUNT")
                                    : string("CMAP_COUNT");
    const auto &current_section = toppar_map.find(section_name);
    if (current_section != toppar_map.end()) {
      if (get<1>(current_section->second).size() == 2) {
        toppar_data.CMAPTermCount = get<1>(current_section->second)[0].Int;
        toppar_data.CMAPTypeCount = get<1>(current_section->second)[1].Int;
        toppar_data.HasCMAP = true;
        toppar_data.CMAPResolution.reserve(toppar_data.CMAPTypeCount);
        // CMAPParameter will be resized later after reading CMAPResolution
        toppar_data.CMAPParameter.reserve(toppar_data.CMAPTypeCount);
        toppar_data.CMAPIndex.reserve(toppar_data.CMAPTermCount * 6);
        get<0>(current_section->second) = true;
      } else {
        std::cerr << "Expect " << 2 << " but " << get<1>(current_section->second).size() << " "
                  << section_name << " are defined\n";
        success = false;
      }
    } else {
      toppar_data.HasCMAP = false;
    }
  }
  if (toppar_data.HasCMAP) {
    // CMAP_RESOLUTION or CHARMM_CMAP_RESOLUTION
    if (success) {
      const string section_name = toppar_data.IsCharmmFF
                                      ? string("CHARMM_CMAP_RESOLUTION")
                                      : string("CMAP_RESOLUTION");
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second),
                      toppar_data.CMAPTypeCount, toppar_data.CMAPResolution,
                      section_name);
        for (int i = 0; i < toppar_data.CMAPTypeCount; ++i) {
          toppar_data.CMAPParameter.push_back(vector<_REAL>());
        }
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
    // CHARMM_CMAP_PARAMETER_N or CMAP_PARAMETER_N
    if (success) {
      for (int i = 0; i < toppar_data.CMAPTypeCount; ++i) {
        string name_index = std::to_string(i + 1);
        if (name_index.size() < 2)
          name_index.insert(0, "0");
        const string section_name =
            (toppar_data.IsCharmmFF ? string("CHARMM_CMAP_PARAMETER_")
                                    : string("CMAP_PARAMETER_")) +
            name_index;
        const auto &current_section = toppar_map.find(section_name);
        if (current_section != toppar_map.end()) {
          const int resolution =
              toppar_data.CMAPResolution[i] * toppar_data.CMAPResolution[i];
          toppar_data.CMAPParameter[i].reserve(resolution);
          success = parse_section(get<1>(current_section->second), resolution,
                        toppar_data.CMAPParameter[i], section_name);
          get<0>(current_section->second) = true;
        } else {
          cerr << "Missing " << section_name << " section!\n";
        }
      }
    }
    // CMAP_INDEX
    if (success) {
      const string section_name = toppar_data.IsCharmmFF
                                      ? string("CHARMM_CMAP_INDEX")
                                      : string("CMAP_INDEX");
      const auto &current_section = toppar_map.find(section_name);
      if (current_section != toppar_map.end()) {
        success = parse_section(get<1>(current_section->second),
                      toppar_data.CMAPTermCount * 6, toppar_data.CMAPIndex,
                      section_name);
        get<0>(current_section->second) = true;
      } else {
        cerr << "Missing " << section_name << " section!\n";
      }
    }
  }
  // NAMD requires the residue IDs for all atoms
  // so I need to fill AtomRes here before return
  if (success) {
    int res = 0;
    for (int i = 0; i < toppar_data.Natom; ++i) {
      if (i + 1 == toppar_data.Ipres[res+1])
        ++res;
      toppar_data.AtomRes.push_back(res);
    }
  }
  // now we check and report any unknown sections
  for (auto it = toppar_map.begin(); it != toppar_map.end(); ++it) {
    if (get<0>(it->second) == false) {
      cerr << "Unknown section " << (it->first).c_str()
           << " found in the AMBER format paramter file!\n";
    }
  }
  return success;
}

bool parse_pointer(const vector<FortranData> &source, Ambertoppar &result) {
  bool success = true;
  if (source.size() > 30) {
    result.Natom = source[0].Int;
    result.Ntypes = source[1].Int;
    result.Nbonh = source[2].Int;
    result.Mbona = source[3].Int;
    result.Ntheth = source[4].Int;
    result.Mtheta = source[5].Int;
    result.Nphih = source[6].Int;
    result.Mphia = source[7].Int;
    result.Nhparm = source[8].Int;
    result.Nparm = source[9].Int;
    result.Nnb = source[10].Int;
    result.Nres = source[11].Int;
    result.Nbona = source[12].Int;
    result.Ntheta = source[13].Int;
    result.Nphia = source[14].Int;
    result.Numbnd = source[15].Int;
    result.Numang = source[16].Int;
    result.Nptra = source[17].Int;
    result.Natyp = source[18].Int;
    result.Nphb = source[19].Int;
    result.Ifpert = source[20].Int;
    result.Nbper = source[21].Int;
    result.Ngper = source[22].Int;
    result.Ndper = source[23].Int;
    result.Mbper = source[24].Int;
    result.Mgper = source[25].Int;
    result.Mdper = source[26].Int;
    result.Ifbox = source[27].Int;
    result.Nmxrs = source[28].Int;
    result.Ifcap = source[29].Int;
    result.Numextra = source[30].Int;
    // Ncopy may not be present
    result.Ncopy = 31 < source.size() ? source[31].Int : 0;
    // "allocate memory"
    const int Ntype2d = result.Ntypes * result.Ntypes;
    const int Nttyp = result.Ntypes * (result.Ntypes + 1) / 2;
    result.AtomNames.reserve(result.Natom);
    result.Charges.reserve(result.Natom);
    result.Masses.reserve(result.Natom);
    result.AtomNumbers.reserve(result.Natom);
    result.Iac.reserve(result.Natom);
    result.Iblo.reserve(result.Natom);
    result.Cno.reserve(Ntype2d);
    result.ResNames.reserve(result.Nres);
    result.Ipres.reserve(result.Nres);
    result.Rk.reserve(result.Numbnd);
    result.Req.reserve(result.Numbnd);
    result.Tk.reserve(result.Numang);
    result.Teq.reserve(result.Numang);
    result.Pk.reserve(result.Nptra);
    result.Pn.reserve(result.Nptra);
    result.Phase.reserve(result.Nptra);
    result.SceeScaleFactor.reserve(result.Nptra);
    result.ScnbScaleFactor.reserve(result.Nptra);
    result.Solty.reserve(result.Natyp);
    result.Cn1.reserve(Nttyp);
    result.Cn2.reserve(Nttyp);
    result.BondHAt1.reserve(result.Nbonh);
    result.BondHAt2.reserve(result.Nbonh);
    result.BondHNum.reserve(result.Nbonh);
    result.BondAt1.reserve(result.Nbona);
    result.BondAt2.reserve(result.Nbona);
    result.BondNum.reserve(result.Nbona);
    result.AngleHAt1.reserve(result.Ntheth);
    result.AngleHAt2.reserve(result.Ntheth);
    result.AngleHAt3.reserve(result.Ntheth);
    result.AngleHNum.reserve(result.Ntheth);
    result.AngleAt1.reserve(result.Ntheta);
    result.AngleAt2.reserve(result.Ntheta);
    result.AngleAt3.reserve(result.Ntheta);
    result.AngleNum.reserve(result.Ntheta);
    result.DihHAt1.reserve(result.Nphih);
    result.DihHAt2.reserve(result.Nphih);
    result.DihHAt3.reserve(result.Nphih);
    result.DihHAt4.reserve(result.Nphih);
    result.DihHNum.reserve(result.Nphih);
    result.DihAt1.reserve(result.Nphia);
    result.DihAt2.reserve(result.Nphia);
    result.DihAt3.reserve(result.Nphia);
    result.DihAt4.reserve(result.Nphia);
    result.DihNum.reserve(result.Nphia);
    result.ExclAt.reserve(result.Nnb);
    result.HB12.reserve(result.Nphb);
    result.HB10.reserve(result.Nphb);
    result.AtomSym.reserve(result.Natom);
    result.AtomTree.reserve(result.Natom);
    result.TreeJoin.reserve(result.Natom);
    result.AtomRes.reserve(result.Natom);
    result.RadiusSet.reserve(1);
    result.Radii.reserve(result.Natom);
    result.Screen.reserve(result.Natom);
  } else {
    success = false;
  }
  return success;
}

bool parse_section(const vector<FortranData> &source, const int &count,
                   vector<string> &destination, const string &section_name) {
  bool success = true;
  if (int(source.size()) != count) {
    std::cerr << "Expect " << count << " but " << source.size() << " "
              << section_name << " are defined\n";
    success = false;
  } else {
    for (int i = 0; i < count; ++i) {
      destination.push_back(source[i].String);
    }
  }
  return success;
}

bool parse_section(const vector<FortranData> &source, const int &count,
                   vector<int> &destination, const string &section_name) {
  bool success = true;
  if (int(source.size()) != count) {
    std::cerr << "Expect " << count << " but " << source.size() << " "
              << section_name << " are defined\n";
    success = false;
  } else {
    for (int i = 0; i < count; ++i) {
      destination.push_back(source[i].Int);
    }
    std::cerr << "Complete reading " << section_name.c_str() << " with " << count << " fields.\n";
  }
  return success;
}

bool parse_section(const vector<FortranData> &source, const int &count,
                   vector<_REAL> &destination, const string &section_name) {
  bool success = true;
  if (int(source.size()) != count) {
    std::cerr << "Expect " << count << " but " << source.size() << " "
              << section_name << " are defined\n";
    success = false;
  } else {
    for (int i = 0; i < count; ++i) {
      destination.push_back(source[i].Real);
    }
    std::cerr << "Complete reading " << section_name.c_str() << " with " << count << " fields.\n";
  }
  return success;
}

bool parse_section(
     const vector<FortranData> &source, const int &count,
     std::initializer_list<std::reference_wrapper<vector<string>>> destination,
     const string &section_name) {
  bool success = true;
  const int total_size = destination.size() * count;
  if (int(source.size()) != total_size) {
    std::cerr << "Expect " << total_size << " but " << source.size() << " "
              << section_name << " are defined\n";
    success = false;
  } else {
    const int stride = destination.size();
    for (int i = 0; i < count; ++i) {
      int j = 0;
      for (auto it_j = destination.begin(); it_j != destination.end(); ++it_j) {
        it_j->get().push_back(source[stride * i + j].String);
        ++j;
      }
    }
    std::cerr << "Complete reading " << section_name.c_str() << " with " << total_size << " fields.\n";
  }
  return success;
}

bool parse_section(
     const vector<FortranData> &source, const int &count,
     std::initializer_list<std::reference_wrapper<vector<int>>> destination,
     const string &section_name) {
  bool success = true;
  const int total_size = destination.size() * count;
  if (int(source.size()) != total_size) {
    std::cerr << "Expect " << total_size << " but " << source.size() << " "
              << section_name << " are defined\n";
    success = false;
  } else {
    const int stride = destination.size();
    for (int i = 0; i < count; ++i) {
      int j = 0;
      for (auto it_j = destination.begin(); it_j != destination.end(); ++it_j) {
        it_j->get().push_back(source[stride * i + j].Int);
        ++j;
      }
    }
    std::cerr << "Complete reading " << section_name.c_str() << " with " << total_size << " fields.\n";
  }
  return success;
}

bool parse_section(
     const vector<FortranData> &source, const int &count,
     std::initializer_list<std::reference_wrapper<vector<_REAL>>> destination,
     const string &section_name) {
  bool success = true;
  const int total_size = destination.size() * count;
  if (int(source.size()) != total_size) {
    std::cerr << "Expect " << total_size << " but " << source.size() << " "
              << section_name << " are defined\n";
    success = false;
  } else {
    const int stride = destination.size();
    for (int i = 0; i < count; ++i) {
      int j = 0;
      for (auto it_j = destination.begin(); it_j != destination.end(); ++it_j) {
        it_j->get().push_back(source[stride * i + j].Real);
        ++j;
      }
    }
    std::cerr << "Complete reading " << section_name.c_str() << " with " << total_size << " fields.\n";
  }
  return success;
}

Ambertoppar readparm(const char *filename) {
  AmberParm7Reader::AmberTopparMap Amber_toppar_map;
  AmberParm7Reader::Ambertoppar data;
  if (AmberParm7Reader::read_amber_parm_stage1(filename, Amber_toppar_map)) {
    if (AmberParm7Reader::read_amber_parm_stage2(Amber_toppar_map, data) == true) {
      data.HasData = true;
    } else {
      std::cerr << "Failed to parse AMBER parm7 file: " << filename << " at read_amber_parm_stage2\n";
    }
  } else {
    std::cerr << "Failed to read AMBER parm7 file: " << filename << " at read_amber_parm_stage1\n";
  }
  return data;
}

void dump_spliter(std::ostream& os) {
  os << "=================================================================================\n";
}

void dump_summary(const Ambertoppar& toppar_data, std::ostream& os) {
  dump_spliter(os);
  os << "Number of atoms: " << toppar_data.Natom << std::endl;
  os << "Number of unique bond types: " << toppar_data.Numbnd << std::endl;
  os << "Number of atoms in the largest residue: " << toppar_data.Nmxrs << std::endl;
  os << "Number of bonds involving hydrogen: " << toppar_data.Nbonh << std::endl;
  os << "Number of bonds not involving hydrogen: " << toppar_data.Nbona << std::endl;
  dump_spliter(os);
  // TODO: more
}

void dump_cmap(const Ambertoppar& toppar_data, std::ostream& os) {
  using std::setw;
  auto ios_flag(os.flags());
  if (toppar_data.HasCMAP) {
    dump_spliter(os);
    os << "CMAP terms: \n";
    for (int i = 0; i < toppar_data.CMAPTermCount; ++i) {
      os << "  ϕ atom serial: " << setw(4) << toppar_data.CMAPIndex[6*i+0]
                                << setw(4) << toppar_data.CMAPIndex[6*i+1]
                                << setw(4) << toppar_data.CMAPIndex[6*i+2]
                                << setw(4) << toppar_data.CMAPIndex[6*i+3];
      os << " ; ";
      os << "ψ atom serial: " << setw(4) << toppar_data.CMAPIndex[6*i+1]
                              << setw(4) << toppar_data.CMAPIndex[6*i+2]
                              << setw(4) << toppar_data.CMAPIndex[6*i+3]
                              << setw(4) << toppar_data.CMAPIndex[6*i+4];
      os << " ; type: " << setw(4) << toppar_data.CMAPIndex[6*i+5] << std::endl;
    }
    dump_spliter(os);
    os << std::setprecision(5);
    os << std::fixed;
    for (int i = 0; i < toppar_data.CMAPTypeCount; ++i) {
      os << "Type " << i+1 << " parameters:\n";
      const int& N = toppar_data.CMAPResolution[i];
      os << "Resolution: " << N << " x " << N << "\n";
      for (int j = 0; j < N; ++j) {
        for (int k = 0; k < N; ++k) {
          os << setw(12) << toppar_data.CMAPParameter[i][j*N+k];
        }
        os << "\n";
      }
      dump_spliter(os);
    }
  }
  os.setf(ios_flag);
}

void dump_atoms(const Ambertoppar& toppar_data, std::ostream& os) {
  using std::setw;
  using std::setprecision;
  auto ios_flag(os.flags());
  dump_spliter(os);
  os << "Atom information: \n";
  for (int i = 0; i < toppar_data.Natom; ++i) {
    os << "  Index = " << setw(8) << i;
    os << " ; name = " << setw(4) << toppar_data.AtomNames[i];
    os << " ; charge = " << setw(12) << setprecision(5) << toppar_data.Charges[i];
    os << " ; mass = " << setw(12) << setprecision(5) << toppar_data.Masses[i];
    os << " ; lj_type_index = " <<  setw(4) << toppar_data.Iac[i];
    os << " ; resid = " <<  setw(8) << toppar_data.AtomRes[i];
    os << " ; resname = " << setw(4) << toppar_data.ResNames[toppar_data.AtomRes[i]];
    os << "\n";
  }
  os.setf(ios_flag);
}

} // namespace AmberParm7Reader

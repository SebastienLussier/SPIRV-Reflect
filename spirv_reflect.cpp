#include "spirv_reflect.hpp"
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace spirv_ref {

template <typename T>
typename std::vector<T>::iterator Find(std::vector<T>& container, const T& value)
{
  typename std::vector<T>::iterator it = std::find(std::begin(container), std::end(container), value);
  return it;
}

template <typename T, class UnaryPredicate>
typename std::vector<T>::iterator FindIf(std::vector<T>& container, UnaryPredicate predicate)
{
  typename std::vector<T>::iterator it = std::find_if(std::begin(container), std::end(container), predicate);
  return it;
}

bool operator<(const Descriptor& a, const Descriptor& b) 
{
    if (a.GetSetNumber() == b.GetSetNumber()) {
      return a.GetBindingNumber() < b.GetBindingNumber();
    }
    return a.GetSetNumber() < b.GetSetNumber();
}

// =================================================================================================
// BlockVariable
// =================================================================================================
BlockVariable::BlockVariable()
{
}

BlockVariable::~BlockVariable()
{
}

// =================================================================================================
// Block
// =================================================================================================
Block::Block()
{
}

Block::~Block()
{
}

// =================================================================================================
// DescriptorBinding
// =================================================================================================
Descriptor::Descriptor()
{
}

Descriptor::Descriptor(ShaderReflection* p_module, uint32_t binding_number_word_offset, uint32_t set_number_word_offset)
  : m_module(p_module),
    m_binding_number_word_offset(binding_number_word_offset),
    m_set_number_word_offset(set_number_word_offset)
{
  if (m_module != nullptr) {
    const uint32_t* p_spirv_code = m_module->GetSpirvCode();
    const uint32_t spirv_word_count = m_module->GetSpirvWordCount();

    assert(m_binding_number_word_offset < spirv_word_count);
    const uint32_t* p_binding_number = (p_spirv_code + m_binding_number_word_offset);
    m_binding_number = *p_binding_number;

    assert(m_set_number_word_offset < spirv_word_count);
    const uint32_t* p_set_number = (p_spirv_code + m_set_number_word_offset);
    m_set_number = *p_set_number;
  }
}

Descriptor::~Descriptor()
{
}

uint32_t Descriptor::GetBindingNumber() const
{
  return m_binding_number;
}

void Descriptor::SetBindingNumber(uint32_t binding_number)
{
  m_binding_number = binding_number;
  if (m_module != nullptr) {
    uint32_t* p_spirv_code = const_cast<uint32_t*>(m_module->GetSpirvCode());
    uint32_t* p_binding_number = (p_spirv_code + m_binding_number_word_offset);
    *p_binding_number = binding_number;
  }
}

uint32_t Descriptor::GetSetNumber() const
{
  return m_set_number;
}

void Descriptor::SetSetNumber(uint32_t set_number)
{
  m_set_number = set_number;
  if (m_module != nullptr) {
    uint32_t* p_spirv_code = const_cast<uint32_t*>(m_module->GetSpirvCode());
    uint32_t* p_set_number = (p_spirv_code + m_set_number_word_offset);
    *p_set_number = set_number;
  }
}

std::string Descriptor::GetInfo() const
{
  std::stringstream ss;
  bool hlsl = false;
  if (hlsl) {
  }
  else {
    ss << "layout(";
    ss << "set=" << m_set_number << ", ";
    ss << "binding=" << m_binding_number;
    ss << ")" << " ";
    ss << "uniform" << " ";
    ss << m_type->GetTypeName() << " ";
    ss << GetName();
    ss << m_type->GetArrayDimString();
    ss << ";";
  }
  return ss.str();
}

// =================================================================================================
// DescriptorSet
// =================================================================================================
DescriptorSet::DescriptorSet()
{
}

DescriptorSet::DescriptorSet(ShaderReflection* p_module, uint32_t set_number)
  : m_module(p_module),
    m_set_number(set_number)
{
}

DescriptorSet::~DescriptorSet()
{
}

uint32_t DescriptorSet::GetSetNumber() const
{
  return m_set_number;
}

void DescriptorSet::SetSetNumber(uint32_t set_number)
{
  m_set_number = set_number;
  for (auto& p_binding : m_bindings) {
    p_binding->SetSetNumber(m_set_number);
  }
  
  if (m_module != nullptr) {
    m_module->SyncDescriptorSets();
  }
}

Descriptor* const DescriptorSet::GetBinding(size_t index)
{
  Descriptor* p_binding = nullptr;
  if (index < m_bindings.size()) {
    p_binding = m_bindings[index];
  }
  return p_binding;
}

// =================================================================================================
// IoVariable
// =================================================================================================
IoVariable::IoVariable()
{
}

IoVariable::~IoVariable()
{
}

// =================================================================================================
// ShaderReflection
// =================================================================================================
ShaderReflection::ShaderReflection()
{
}

ShaderReflection::ShaderReflection(size_t size, void* p_code)
  : m_spirv_size(size),
    m_spirv_code(static_cast<uint32_t*>(p_code)),
    m_spirv_word_count(static_cast<uint32_t>(size / 4))
{
}

ShaderReflection::~ShaderReflection()
{
  // Destroy descriptor sets
  for (auto& p_descriptor_set : m_descriptor_sets) {
    if (p_descriptor_set != nullptr) {
      delete p_descriptor_set;
      p_descriptor_set = nullptr;
    }
  }
  m_descriptor_sets.clear();
}

detail::Type* const ShaderReflection::GetType(size_t index)
{
  detail::Type* p_type = nullptr;
  if (index < m_types.size()) {
    p_type = &(m_types[index]);
  }
  return p_type;
}

Descriptor* const ShaderReflection::GetDescriptor(size_t index)
{
  Descriptor* p_descriptor = nullptr;
  if (index < m_descriptors.size()) {
    p_descriptor = &(m_descriptors[index]);
  }
  return p_descriptor;
}

DescriptorSet* const ShaderReflection::GetDescriptorSet(size_t index)
{
  DescriptorSet* p_descriptor_set = nullptr;
  if (index < m_descriptor_sets.size()) {
    p_descriptor_set = m_descriptor_sets[index];
  }
  return p_descriptor_set;
}

Result ShaderReflection::SyncDescriptorSets()
{
  Result result = SUCCESS;

  for (auto& p_descriptor_set : m_descriptor_sets) {
    p_descriptor_set->m_bindings.clear();
  }

  for (auto& descriptor : m_descriptors) {
    // Find or create descriptor set
    DescriptorSet* p_descriptor_set = nullptr;
    {
      uint32_t set_number = descriptor.GetSetNumber();
      auto it = FindIf(m_descriptor_sets,
                       [set_number](const DescriptorSet* p_elem) -> bool {
                         return p_elem->GetSetNumber() == set_number; });
      if (it != std::end(m_descriptor_sets)) {
        p_descriptor_set = *it;
      }
      else {
        p_descriptor_set = new DescriptorSet(this, set_number);
        m_descriptor_sets.push_back(p_descriptor_set);
      }
    }

    // Duplicate binding check
    {
      uint32_t binding_number = descriptor.GetBindingNumber();
      auto it = FindIf(p_descriptor_set->m_bindings,
                       [binding_number](const Descriptor* p_elem) -> bool {
                         return p_elem->GetBindingNumber() == binding_number; });
      if (it != std::end(p_descriptor_set->m_bindings)) {
        result = ERROR_DUPLICATE_BINDING;
      }
    }

    if (result != SUCCESS) {
      break;
    }

    p_descriptor_set->m_bindings.push_back(&descriptor);
  }

  if (result == SUCCESS) {
    for (auto& p_descriptor_set : m_descriptor_sets) {
      std::sort(std::begin(p_descriptor_set->m_bindings),
                std::end(p_descriptor_set->m_bindings),
                [](const Descriptor* a, const Descriptor* b) -> bool { return *a < *b; });
    }
  }

  return result;
}

detail::Type* const ShaderReflection::FindType(uint32_t type_id)
{
  detail::Type* p_type = nullptr;
  auto it = FindIf(m_types, [type_id](const detail::Type& elem) -> bool { return elem.m_id == type_id; });
  if (it != std::end(m_types)) {
    p_type = &(*it);
  }
  return p_type;
}

// =================================================================================================
// Standalone functions
// =================================================================================================
Result ParseShaderReflection(size_t size, void* p_code, ShaderReflection* p_module)
{
  detail::Parser parser(static_cast<uint32_t>(size), static_cast<uint32_t*>(p_code), p_module);
  Result result = parser.GetResult();
  return result;
}

namespace detail {

enum {
  STARTING_WORD_INDEX = 5,
  WORD_SIZE           = sizeof(uint32_t),
  MINIMUM_FILE_SIZE   = STARTING_WORD_INDEX * WORD_SIZE
};

uint32_t RoundUp(uint32_t value, uint32_t multiple) 
{
  assert(multiple && ((multiple & (multiple - 1)) == 0));
  return (value + multiple - 1) & ~(multiple - 1);
}

// =================================================================================================
// Type
// =================================================================================================
Type::Type()
{
}

Type::Type(ShaderReflection* p_module, uint32_t id, spv::Op op)
  : m_module(p_module),
    m_id(id),
    m_op(op)
{
}

void Type::BuildTypeStrings()
{
  for (auto& member : m_members) {
    member.BuildTypeStrings();
  }

  if (m_type_name.empty()) {
    std::stringstream ss;
    bool hlsl = false;
    // HLSL format
    if (hlsl) {
      // Component
      switch (m_type_flags & TYPE_FLAG_COMPONENT) {
        default: break;
        case TYPE_FLAG_COMPONENT_VOID  : ss << "void"; break;
        case TYPE_FLAG_COMPONENT_BOOL  : ss << "bool"; break;
        case TYPE_FLAG_COMPONENT_INT   : ss << ((m_scalar_traits.signedness == 1) ? "int" : "uint"); break;
        case TYPE_FLAG_COMPONENT_FLOAT : ss << "float"; break;
      }
      // Composite
      switch (m_type_flags & TYPE_FLAG_COMPOSITE) {
        default: break;
        case TYPE_FLAG_COMPOSITE_VECTOR : ss << m_vector_traits.component_count; break;
        case TYPE_FLAG_COMPOSITE_MATRIX : ss << m_matrix_traits.column_count << "x" << m_matrix_traits.row_count; break;
      }
    }
    // GLSL format
    else {
      if ((m_type_flags & TYPE_FLAG_POINTER) && (!(m_type_flags & TYPE_FLAG_STRUCTURE))) {
        ss << "ptr" << " ";
      }
      // Composite
      switch (m_type_flags & TYPE_FLAG_COMPOSITE) {
        default: {
          if (m_type_flags & TYPE_FLAG_COMPONENT) {
            switch (m_type_flags & TYPE_FLAG_COMPONENT) {
              default: break;
              case TYPE_FLAG_COMPONENT_VOID  : ss << "void"; break;
              case TYPE_FLAG_COMPONENT_BOOL  : ss << "bool"; break;
              case TYPE_FLAG_COMPONENT_INT   : ss << ((m_scalar_traits.signedness == 1) ? "int" : "uint"); break;
              case TYPE_FLAG_COMPONENT_FLOAT : ss << "float"; break;
            }
          }
          else if(m_type_flags & TYPE_FLAG_EXTERNAL) {
            bool write_dim = false;
            uint32_t masked = (m_type_flags & TYPE_FLAG_EXTERNAL);
            if (masked == TYPE_FLAG_EXTERNAL_IMAGE) {
              ss << "texture";
              write_dim = true;
            }
            else if (masked == TYPE_FLAG_EXTERNAL_SAMPLER) {
              ss << "sampler";
            }
            else if (masked == (TYPE_FLAG_EXTERNAL_IMAGE | TYPE_FLAG_EXTERNAL_SAMPLER)) {
              ss << "sampler";
              write_dim = true;
            }
            else if (masked == (TYPE_FLAG_EXTERNAL_IMAGE | TYPE_FLAG_EXTERNAL_BUFFER)) {
              if (m_image_traits.image_format == spv::ImageFormatUnknown) {
                ss << "samplerBuffer";
              }
              else {
                ss << "imageBuffer";
              }
            }
            
            if (write_dim) {
              switch (m_image_traits.dim) {
                case spv::Dim1D   : ss << "1D"; break;
                case spv::Dim2D   : ss << "2D"; break;
                case spv::Dim3D   : ss << "3D"; break;
                case spv::DimCube : ss << "Cube"; break;
                case spv::DimRect : ss << "Rect"; break;
              }
            }
          }
        }
        break;

        case TYPE_FLAG_COMPOSITE_VECTOR: {
          switch (m_type_flags & TYPE_FLAG_COMPONENT) {
            default: break;
            case TYPE_FLAG_COMPONENT_BOOL  : ss << "bvec"; break;
            case TYPE_FLAG_COMPONENT_INT   : ss << ((m_scalar_traits.signedness == 1) ? "ivec" : "uvec"); break;
            case TYPE_FLAG_COMPONENT_FLOAT : ss << ((m_scalar_traits.width == 64) ? "dvec" : "vec"); break;
          }
          ss << m_vector_traits.component_count;
        }
        break;

        case TYPE_FLAG_COMPOSITE_MATRIX: {
          switch (m_type_flags & TYPE_FLAG_COMPONENT) {
            default: break;
            case TYPE_FLAG_COMPONENT_BOOL  : ss << "bmat"; break;
            case TYPE_FLAG_COMPONENT_INT   : ss << ((m_scalar_traits.signedness == 1) ? "imat" : "umat"); break;
            case TYPE_FLAG_COMPONENT_FLOAT : ss << ((m_scalar_traits.width == 64) ? "dmat" : "mat"); break;
          }
          if (m_matrix_traits.column_count == m_matrix_traits.row_count) {
            ss << m_matrix_traits.column_count << "x" << m_matrix_traits.row_count;
          }
          else {
            ss << m_matrix_traits.column_count;
          }
        }
        break;
      }     
    }
    m_type_name = ss.str();
  }

  
  if (m_array_string.empty()) {
    // Runtime Array
    if (m_type_flags & TYPE_FLAG_COMPOSITE_RUNTIME_ARRAY) {
      m_array_string = "[]";
    }
    // Array
    else if (!m_array.empty()) {
      std::stringstream ss;
      for (auto& value : m_array) {
        ss << "[" << value << "]";
      }
      m_array_string = ss.str();
    }
  }
}

Type::~Type()
{
}

const std::string& Type::GetTypeName() const
{
  return m_type_name;
}

const std::string& Type::GetArrayDimString() const
{
  return m_array_string;
}

const std::string& Type::GetBlockMemberName() const
{
  return m_block_member_name;
}

std::string Type::GetInfo(const std::string& indent) const
{
  std::stringstream ss;
  if (m_type_flags & TYPE_FLAG_STRUCTURE) {
    const std::string& t = indent;
    struct Entry {
      std::string deco;
      std::string type;
      std::string name;
      std::string array;
    };
    size_t deco_width = 0;
    size_t type_width = 0;
    size_t name_width = 0;
    std::vector<Entry> entries;
    for (const auto& member : m_members) {
      Entry e;
      e.type  = member.GetTypeName();
      e.name  = member.GetBlockMemberName();
      e.array = member.GetArrayDimString();
      type_width = std::max(type_width, e.type.length());
      name_width = std::max(name_width, e.name.length());
      entries.push_back(e);
    }

    if (m_type_flags & TYPE_FLAG_POINTER) {
      ss << "ptr" << " ";
    }

    ss << "struct" << " " << GetTypeName() << " " << "{" << "\n";
    for (const auto& e : entries) {
      ss << t;
      ss << std::setw(type_width) << std::left << e.type;
      ss << " " << e.name << e.array;
      ss << ";";
      ss << "\n";
    }
    ss << "};";
  }
  else {
    ss << GetTypeName();
  }
  return ss.str();
}

// =================================================================================================
// Parser
// =================================================================================================
Parser::Parser(size_t size, const uint32_t* p_spirv_code, ShaderReflection* p_module)
{
  m_result = ((size % 4) == 0) && (size >= MINIMUM_FILE_SIZE) ? SUCCESS : ERROR_INVALID_SPIRV_SIZE;

  if (m_result == SUCCESS) {
    m_spirv_code = p_spirv_code;
    m_spirv_word_count = static_cast<uint32_t>(size / 4);

    m_module = p_module;
    m_module->m_spirv_size = size;
    m_module->m_spirv_code = const_cast<uint32_t*>(p_spirv_code);
    m_module->m_spirv_word_count = m_spirv_word_count;

    m_result = ParseNodes();
  }

  if (m_result == SUCCESS) {
    m_result = ParseNames();
  }

  if (m_result == SUCCESS) {
    m_result = ParseDecorations();
  }

  if (m_result == SUCCESS) {
    m_result = ParseTypes(&(m_module->m_types));
  }

  if (m_result == SUCCESS) {
    m_result = ParseDescriptors(&(m_module->m_descriptors));
    std::sort(std::begin(m_module->m_descriptors), std::end(m_module->m_descriptors));
  }

  if (m_result == SUCCESS) {
    m_module->SyncDescriptorSets();
  }
}

Parser::~Parser()
{
}

Parser::Node* Parser::FindNode(uint32_t node_id) {
  Parser::Node* found_node = nullptr;

  auto it = std::find_if(std::begin(m_nodes), std::end(m_nodes), 
                         [node_id](const Node& elem) -> bool { return elem.result_id == node_id; });

  if (it != std::end(m_nodes)) {
    found_node = &(*it);
  }
  return found_node;
}
  
Parser::Node* Parser::ResolveType(Node* p_node) {
  Parser::Node* resolved_node = p_node;
  while (resolved_node->type_id != 0) {
    resolved_node = FindNode(resolved_node->type_id);
  }
  return resolved_node;
}

Result Parser::ParseNodes()
{
  Result result = SUCCESS;
  size_t spirv_word_index = STARTING_WORD_INDEX;

  while (spirv_word_index < m_spirv_word_count) {
    uint32_t word = m_spirv_code[spirv_word_index];  
    uint32_t op = word & 0xFFFF;

    Node node = Node();
    node.op = static_cast<spv::Op>(op);
    node.word_offset = static_cast<uint32_t>(spirv_word_index);
    node.word_count = (word >> 16) & 0xFFFF;

    switch (node.op) {
      default: break;
    
      case spv::OpEntryPoint: {
        spv::ExecutionModel execution_model = static_cast<spv::ExecutionModel>(*(m_spirv_code + node.word_offset + 1));
        uint32_t entry_point = *(m_spirv_code + node.word_offset + 2);
        std::string name = reinterpret_cast<const char*>(m_spirv_code + node.word_offset + 3);
        uint32_t name_word_count = detail::RoundUp(static_cast<uint32_t>(name.length()), WORD_SIZE) / WORD_SIZE; 
        uint32_t var_word_index = 3 + name_word_count;
        for (; var_word_index < node.word_count; ++var_word_index) {
          uint32_t var_id = *(m_spirv_code + node.word_offset + var_word_index);
          m_io_var_ids.push_back(var_id);
        }
      }
      break;
    
      case spv::OpTypeVoid:
      case spv::OpTypeBool:
      case spv::OpTypeInt:
      case spv::OpTypeFloat:
      case spv::OpTypeVector:
      case spv::OpTypeMatrix:
      case spv::OpTypeSampler:
      case spv::OpTypeRuntimeArray:
      case spv::OpTypeStruct:
      case spv::OpTypeOpaque:
      case spv::OpTypeFunction:
      case spv::OpTypeEvent:
      case spv::OpTypeDeviceEvent:
      case spv::OpTypeReserveId:
      case spv::OpTypeQueue:
      case spv::OpTypePipe:
      case spv::OpTypeForwardPointer: 
      {
        node.result_id = *(m_spirv_code + node.word_offset + 1);
        node.is_type = true;
      }
      break;

      case spv::OpTypeImage: {
        node.result_id              = *(m_spirv_code + node.word_offset + 1);
        node.image_traits.sampled_type_id  = *(m_spirv_code + node.word_offset + 2);
        node.image_traits.dim              = static_cast<spv::Dim>(*(m_spirv_code + node.word_offset + 3));
        node.image_traits.depth            = *(m_spirv_code + node.word_offset + 4);
        node.image_traits.arrayed          = *(m_spirv_code + node.word_offset + 5);
        node.image_traits.ms               = *(m_spirv_code + node.word_offset + 6);
        node.image_traits.sampled          = *(m_spirv_code + node.word_offset + 7);
        node.image_traits.image_format     = static_cast<spv::ImageFormat>(*(m_spirv_code + node.word_offset + 8));
        node.is_type = true;
      }
      break;

      case spv::OpTypeSampledImage: {
        node.result_id      = *(m_spirv_code + node.word_offset + 1);
        node.image_type_id  = *(m_spirv_code + node.word_offset + 2);
        node.is_type = true;
      }
      break;

      case spv::OpTypeArray:  {
        node.result_id              = *(m_spirv_code + node.word_offset + 1);
        node.array_traits.element_type_id  = *(m_spirv_code + node.word_offset + 2);
        node.array_traits.length_id        = *(m_spirv_code + node.word_offset + 3);
        node.is_type = true;
      }
      break;

      case spv::OpTypePointer: {
        node.result_id     = *(m_spirv_code + node.word_offset + 1);
        node.storage_class = static_cast<spv::StorageClass>(*(m_spirv_code + node.word_offset + 2));
        node.type_id       = *(m_spirv_code + node.word_offset + 3);
        node.is_type = true;
      }
      break;

      case spv::OpConstantTrue:
      case spv::OpConstantFalse:
      case spv::OpConstant:
      case spv::OpConstantComposite:
      case spv::OpConstantSampler:
      case spv::OpConstantNull: {
        node.result_type_id = *(m_spirv_code + node.word_offset + 1);
        node.result_id      = *(m_spirv_code + node.word_offset + 2);
      }
      break;

      case spv::OpVariable:
      {
        node.type_id   = *(m_spirv_code + node.word_offset + 1);
        node.result_id = *(m_spirv_code + node.word_offset + 2);
      }
      break;
    }

    m_nodes.push_back(node);
    spirv_word_index += node.word_count;
  }

  return result;
}

spirv_ref::Result Parser::ParseNames()
{
  Result result = SUCCESS;
  for (const auto& node : m_nodes) {
    if ((node.op != spv::OpName) && (node.op != spv::OpMemberName)) {
      continue;
    }

    // Not all nodes get parsed, so FindNode returning nullptr is expected.
    uint32_t target_id = *(m_spirv_code + node.word_offset + 1);
    auto p_target_node = FindNode(target_id);
    if (p_target_node == nullptr) {
      continue;
    }
    std::string* p_target_name = &(p_target_node->name);

    uint32_t member_offset = 0;
    if (node.op == spv::OpMemberName) {
      member_offset = 1;
      uint32_t member_index = *(m_spirv_code + node.word_offset + 2);
      p_target_name = &(p_target_node->member_names[member_index]);
    }

    *p_target_name = reinterpret_cast<const char*>(m_spirv_code + node.word_offset + member_offset + 2);
  }
  return result;
}

Result Parser::ParseDecorations()
{
  Result result = SUCCESS;
  for (const auto& node : m_nodes) {
    if ((node.op != spv::OpDecorate) && (node.op != spv::OpMemberDecorate)) {
     continue;
    }

    uint32_t target_id = *(m_spirv_code + node.word_offset + 1);
    auto p_target_node = FindNode(target_id);
    if (p_target_node == nullptr) {
      result = ERROR_INVALID_ID_REFERENCE;
      break;
    }
    VariableDecorations* p_target_decorations = &(p_target_node->decorations);

    uint32_t member_offset = 0;
    if (node.op == spv::OpMemberDecorate) {
      member_offset = 1;
      uint32_t member_index = *(m_spirv_code + node.word_offset + 2);
      p_target_decorations = &(p_target_node->member_decorations[member_index]);
    }

    spv::Decoration decoration = static_cast<spv::Decoration>(*(m_spirv_code + node.word_offset + member_offset + 2));
    switch (decoration) {
      default: break;

      case spv::DecorationBlock: {
        p_target_decorations->type.attrs |= TYPE_ATTR_BLOCK;
      }
      break;

      case spv::DecorationBufferBlock: {
        p_target_decorations->type.attrs |= TYPE_ATTR_BUFFER_BLOCK;
      }
      break;

      case spv::DecorationColMajor: {
        p_target_decorations->type.attrs |= TYPE_ATTR_COL_MAJOR;
      }
      break;

      case spv::DecorationRowMajor: {
        p_target_decorations->type.attrs |= TYPE_ATTR_ROW_MAJOR;
      }
      break;

      case spv::DecorationArrayStride: {
        uint32_t word_offset = node.word_offset + member_offset + 3;
        uint32_t value = *(m_spirv_code + word_offset);
        p_target_decorations->type.array_stride = value;
      }
      break;

      case spv::DecorationMatrixStride: {
        uint32_t word_offset = node.word_offset + member_offset + 3;
        uint32_t value = *(m_spirv_code + word_offset);
        p_target_decorations->type.matrix_stride = value;
      }
      break;

      case spv::DecorationBuiltIn: {
        p_target_decorations->type.attrs |= TYPE_ATTR_BUILT_IN;
      }
      break;

      case spv::DecorationNoPerspective: {
        p_target_decorations->type.attrs |= TYPE_ATTR_NO_PERSPECTIVE;
      }
      break;

      case spv::DecorationFlat: {
        p_target_decorations->type.attrs |= TYPE_ATTR_FLAT;
      }
      break;

      case spv::DecorationLocation: {
        uint32_t word_offset = node.word_offset + member_offset+ 3;
        uint32_t value = *(m_spirv_code + word_offset);
        p_target_decorations->location = { word_offset, value };
      }
      break;   

      case spv::DecorationBinding: {
        uint32_t word_offset = node.word_offset + member_offset+ 3;
        uint32_t value = *(m_spirv_code + word_offset);
        p_target_decorations->binding = { word_offset, value };
      }
      break;

      case spv::DecorationDescriptorSet: {
        uint32_t word_offset = node.word_offset + member_offset+ 3;
        uint32_t value = *(m_spirv_code + word_offset);
        p_target_decorations->set = { word_offset, value };
      }
      break;

      case spv::DecorationOffset: {
        uint32_t word_offset = node.word_offset + member_offset+ 3;
        uint32_t value = *(m_spirv_code + word_offset);
        p_target_decorations->offset = { word_offset, value };
      }
      break;      
    }
  }
  return result;
}

Result Parser::ParseTypes(std::vector<Type>* p_types)
{
  Result result = SUCCESS;

  for (auto& node : m_nodes) {
    if ((!node.is_type) || (node.op == spv::OpTypeFunction)) {
     continue;
    }

    auto it = FindIf(*p_types, [id = node.result_id](const Type& elem) -> bool { return elem.m_id == id; });
    if (it != std::end(*p_types)) {
      result = ERROR_DUPLICATE_TYPE;
      break;
    }

    Type type = Type(m_module, node.result_id, node.op);
    result = ParseType(&node, &type);
    assert(result == SUCCESS);
    if (result != SUCCESS) {
      break;
    }

    type.BuildTypeStrings();
    
    p_types->push_back(type);
  }

  return result;
}

Result Parser::ParseType(Node* p_node, Type* p_type)
{
  Result result = SUCCESS;
  switch (p_node->op) {
    default: break;
    case spv::OpTypeVoid: {
      p_type->m_type_flags |= TYPE_FLAG_COMPONENT_VOID;
    }
    break;

    case spv::OpTypeBool: {
      p_type->m_type_flags |= TYPE_FLAG_COMPONENT_BOOL;
    }
    break; 
      
    case spv::OpTypeInt: {
      p_type->m_type_flags |= TYPE_FLAG_COMPONENT_INT;
      p_type->m_scalar_traits.width = *(m_spirv_code + p_node->word_offset + 2);
      p_type->m_scalar_traits.signedness = *(m_spirv_code + p_node->word_offset + 3);
    }
    break;

    case spv::OpTypeFloat: {
      p_type->m_type_flags |= TYPE_FLAG_COMPONENT_FLOAT;
      p_type->m_scalar_traits.width = *(m_spirv_code + p_node->word_offset + 2);
    }
    break;

    case spv::OpTypeVector: { 
      p_type->m_type_flags |= TYPE_FLAG_COMPOSITE_VECTOR;
      uint32_t component_type_id = *(m_spirv_code + p_node->word_offset + 2);
      p_type->m_vector_traits.component_count =*(m_spirv_code + p_node->word_offset + 3);
      // Parse component type
      Node* p_next_node = FindNode(component_type_id);
      if (p_next_node != nullptr) {
        result = ParseType(p_next_node, p_type);
      }
      else {
        result = ERROR_INVALID_ID_REFERENCE;
      }
    }
    break;

    case spv::OpTypeMatrix: {
      p_type->m_type_flags |= TYPE_FLAG_COMPOSITE_MATRIX;
      uint32_t column_type_id = *(m_spirv_code + p_node->word_offset + 2);
      Node* p_next_node = FindNode(column_type_id);
      if (p_next_node != nullptr) {
        result = ParseType(p_next_node, p_type);
      }
      else {
        result = ERROR_INVALID_ID_REFERENCE;
      }
    }
    break;

    case spv::OpTypeImage: {
      p_type->m_type_flags |= TYPE_FLAG_EXTERNAL_IMAGE;
      p_type->m_image_traits.dim          = static_cast<spv::Dim>(*(m_spirv_code + p_node->word_offset + 3));
      p_type->m_image_traits.depth        = *(m_spirv_code + p_node->word_offset + 4);
      p_type->m_image_traits.arrayed      = *(m_spirv_code + p_node->word_offset + 5);
      p_type->m_image_traits.ms           = *(m_spirv_code + p_node->word_offset + 6);
      p_type->m_image_traits.sampled      = *(m_spirv_code + p_node->word_offset + 7);
      p_type->m_image_traits.image_format = static_cast<spv::ImageFormat>(*(m_spirv_code + p_node->word_offset + 8));
    }
    break;

    case spv::OpTypeSampler: {
      p_type->m_type_flags |= TYPE_FLAG_EXTERNAL_SAMPLER;
    }
    break;

    case spv::OpTypeSampledImage: {
      p_type->m_type_flags |= TYPE_FLAG_EXTERNAL_IMAGE;
      uint32_t image_type_id = *(m_spirv_code + p_node->word_offset + 2);
      Node* p_next_node = FindNode(image_type_id);
      if (p_next_node != nullptr) {
        result = ParseType(p_next_node, p_type);
      }
      else {
        result = ERROR_INVALID_ID_REFERENCE;
      }
      if (result == SUCCESS) {
        if (p_type->m_image_traits.dim == spv::DimBuffer) {
          p_type->m_type_flags |= TYPE_FLAG_EXTERNAL_BUFFER;
        }
        else {
          p_type->m_type_flags |= TYPE_FLAG_EXTERNAL_SAMPLER;
        }
      }
    }
    break;

    case spv::OpTypeArray: {
      p_type->m_type_flags |= TYPE_FLAG_COMPOSITE_ARRAY;
      uint32_t element_type_id = *(m_spirv_code + p_node->word_offset + 2);
      uint32_t length_id = *(m_spirv_code + p_node->word_offset + 3);
      // Get length for current dimension
      Node* p_length_node = FindNode(length_id);
      if (p_length_node != nullptr) {
        uint32_t length = *(m_spirv_code + p_length_node->word_offset + 3);
        p_type->m_array.push_back(length);
        // Parse next dimension or element type
        Node* p_next_node = FindNode(element_type_id);
        if (p_next_node != nullptr) {
          result = ParseType(p_next_node, p_type);
        }
        else {
          result = ERROR_INVALID_ID_REFERENCE;
        }
      }
      else {
        result = ERROR_INVALID_ID_REFERENCE;
      }
    }
    break;

    case spv::OpTypeRuntimeArray: {
      p_type->m_type_flags |= TYPE_FLAG_COMPOSITE_RUNTIME_ARRAY;
      uint32_t element_type_id = *(m_spirv_code + p_node->word_offset + 2);
      // Parse next dimension or element type
      Node* p_next_node = FindNode(element_type_id);
      if (p_next_node != nullptr) {
        result = ParseType(p_next_node, p_type);
      }
      else {
        result = ERROR_INVALID_ID_REFERENCE;
      }
    }
    break;

    case spv::OpTypeStruct: {
      p_type->m_type_flags |= TYPE_FLAG_STRUCTURE;
      uint32_t word_index = 2;
      uint32_t member_index = 0;
      for (; word_index < p_node->word_count; ++word_index, ++member_index) {
        uint32_t member_id = *(m_spirv_code + p_node->word_offset + word_index);
        // Find member node
        Node* p_member_node = FindNode(member_id);
        if (p_member_node == nullptr) {
          result = ERROR_INVALID_ID_REFERENCE;
          break;
        }
        // Parse member type
        Type member_type(m_module, member_id, p_member_node->op);
        result = ParseType(p_member_node, &member_type);
        if (result != SUCCESS) {
          break;
        }
        // Append to member
        if (p_node->member_names.find(member_index) != p_node->member_names.end()) {
          member_type.m_block_member_name = p_node->member_names[member_index];
        }
        p_type->m_members.push_back(member_type);
      }
    }
    break;

    case spv::OpTypeOpaque: break;

    case spv::OpTypePointer: {
      p_type->m_type_flags |= TYPE_FLAG_POINTER;
      p_type->m_storage_class = static_cast<spv::StorageClass>(*(m_spirv_code + p_node->word_offset + 2));
      uint32_t type_id = *(m_spirv_code + p_node->word_offset + 3);
      // Parse type
      Node* p_next_node = FindNode(type_id);
      if (p_next_node != nullptr) {
        result = ParseType(p_next_node, p_type);
      }
      else {
        result = ERROR_INVALID_ID_REFERENCE;
      }
    }
    break;
  }

  if (result == SUCCESS) {
    // Pointer
    if (p_type->m_type_name.empty()) {
      p_type->m_type_name = p_node->name;
    }
  }

  return result;
}

Result Parser::ParseDescriptors(std::vector<Descriptor>* p_descriptors)
{
  assert(p_descriptors != nullptr);

  Result result = SUCCESS;

  for (auto& node : m_nodes) {
    if (node.op != spv::OpVariable) {
      continue;
    }

    if ((node.decorations.set.value == UINT32_MAX) || (node.decorations.binding.value == UINT32_MAX)) {
      continue;
    }

    const uint32_t binding_number = node.decorations.binding.value;
    const uint32_t set_number = node.decorations.set.value;

    auto it = FindIf(*p_descriptors, 
                      [binding_number,set_number](const Descriptor& elem) -> bool { 
                        return (elem.GetBindingNumber() == binding_number) && 
                                (elem.GetSetNumber() == set_number); } );

    if (it != std::end(*p_descriptors)) {
      result = ERROR_DUPLICATE_BINDING;
      break;
    }

    // Find the first non-pointer node
    Node* p_type_node = FindNode(node.type_id);
    result = (p_type_node == nullptr) ? ERROR_INVALID_ID_REFERENCE : result;
    while ((p_type_node->op == spv::OpTypePointer) && (result == SUCCESS)) {
        p_type_node = FindNode(p_type_node->type_id);
        result = (p_type_node == nullptr) ? ERROR_INVALID_ID_REFERENCE : result;
    }
    if (result != SUCCESS) {
      break;
    }

    Type* p_type = m_module->FindType(p_type_node->result_id);
    if (p_type == nullptr) {
      result = ERROR_INVALID_ID_REFERENCE;
      break;
    }

    Descriptor descriptor = Descriptor(m_module, 
                                       node.decorations.binding.word_offset, 
                                       node.decorations.set.word_offset);
    descriptor.m_type = p_type;
    descriptor.m_name = node.name;
    p_descriptors->push_back(descriptor);
  }

  return result;
}

} // namespace detail
} // namespace spirv_ref
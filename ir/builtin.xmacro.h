IR_BUILTIN_MACRO(Foreign, "foreign", type::Generic)
IR_BUILTIN_MACRO(Opaque, "opaque", type::Func({}, {type::Type_}))
IR_BUILTIN_MACRO(Bytes, "bytes", type::Func({type::Type_}, {type::Int64}))
IR_BUILTIN_MACRO(Alignment, "alignment",
                 type::Func({type::Type_}, {type::Int64}))
#if defined(ICARUS_DEBUG)
IR_BUILTIN_MACRO(DebugIr, "debug_ir", type::Func({}, {}))
#endif  // defined(ICARUS_DEBUG)

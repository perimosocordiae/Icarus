OP_MACRO(Death,                Death,               void,                    empty_)
OP_MACRO(Bytes,                Bytes,               type::Type const*,       type_arg_)
OP_MACRO(Align,                Align,               type::Type const*,       type_arg_)
OP_MACRO(NotBool,              Not,                 bool,                    reg_)
OP_MACRO(NotFlags,             Not,                 FlagsVal,                typed_reg_)
OP_MACRO(NegInt8,              Neg,                 int8_t,                  reg_)
OP_MACRO(NegInt16,             Neg,                 int16_t,                 reg_)
OP_MACRO(NegInt32,             Neg,                 int32_t,                 reg_)
OP_MACRO(NegInt64,             Neg,                 int64_t,                 reg_)
OP_MACRO(NegFloat32,           Neg,                 float,                   reg_)
OP_MACRO(NegFloat64,           Neg,                 double,                  reg_)
OP_MACRO(LoadBool,             Load,                bool,                    addr_arg_)
OP_MACRO(LoadInt8,             Load,                int8_t,                  addr_arg_)
OP_MACRO(LoadInt16,            Load,                int16_t,                 addr_arg_)
OP_MACRO(LoadInt32,            Load,                int32_t,                 addr_arg_)
OP_MACRO(LoadInt64,            Load,                int64_t,                 addr_arg_)
OP_MACRO(LoadNat8,             Load,                uint8_t,                 addr_arg_)
OP_MACRO(LoadNat16,            Load,                uint16_t,                addr_arg_)
OP_MACRO(LoadNat32,            Load,                uint32_t,                addr_arg_)
OP_MACRO(LoadNat64,            Load,                uint64_t,                addr_arg_)
OP_MACRO(LoadFloat32,          Load,                float,                   addr_arg_)
OP_MACRO(LoadFloat64,          Load,                double,                  addr_arg_)
OP_MACRO(LoadType,             Load,                type::Type const*,       addr_arg_)
OP_MACRO(LoadEnum,             Load,                EnumVal,                 addr_arg_)
OP_MACRO(LoadFlags,            Load,                FlagsVal,                addr_arg_)
OP_MACRO(LoadAddr,             Load,                ir::Addr,                addr_arg_)
OP_MACRO(LoadFunc,             Load,                AnyFunc,                 addr_arg_)
OP_MACRO(LoadInterface,        Load,                type::Interface const*,  addr_arg_)
OP_MACRO(StoreBool,            Store,               bool,                    store_bool_)
OP_MACRO(StoreInt8,            Store,               int8_t,                  store_i8_)
OP_MACRO(StoreInt16,           Store,               int16_t,                 store_i16_)
OP_MACRO(StoreInt32,           Store,               int32_t,                 store_i32_)
OP_MACRO(StoreInt64,           Store,               int64_t,                 store_i64_)
OP_MACRO(StoreNat8,            Store,               uint8_t,                 store_u8_)
OP_MACRO(StoreNat16,           Store,               uint16_t,                store_u16_)
OP_MACRO(StoreNat32,           Store,               uint32_t,                store_u32_)
OP_MACRO(StoreNat64,           Store,               uint64_t,                store_u64_)
OP_MACRO(StoreFloat32,         Store,               float,                   store_float32_)
OP_MACRO(StoreFloat64,         Store,               double,                  store_float64_)
OP_MACRO(StoreType,            Store,               type::Type const*,       store_type_)
OP_MACRO(StoreEnum,            Store,               EnumVal,                 store_enum_)
OP_MACRO(StoreFunc,            Store,               ir::AnyFunc,             store_func_)
OP_MACRO(StoreFlags,           Store,               FlagsVal,                store_flags_)
OP_MACRO(StoreAddr,            Store,               ir::Addr,                store_addr_)
OP_MACRO(PrintBool,            Print,               bool,                    bool_arg_)
OP_MACRO(PrintInt8,            Print,               int8_t,                  i8_arg_)
OP_MACRO(PrintInt16,           Print,               int16_t,                 i16_arg_)
OP_MACRO(PrintInt32,           Print,               int32_t,                 i32_arg_)
OP_MACRO(PrintInt64,           Print,               int64_t,                 i64_arg_)
OP_MACRO(PrintNat8,            Print,               uint8_t,                 u8_arg_)
OP_MACRO(PrintNat16,           Print,               uint16_t,                u16_arg_)
OP_MACRO(PrintNat32,           Print,               uint32_t,                u32_arg_)
OP_MACRO(PrintNat64,           Print,               uint64_t,                u64_arg_)
OP_MACRO(PrintFloat32,         Print,               float,                   float32_arg_)
OP_MACRO(PrintFloat64,         Print,               double,                  float64_arg_)
OP_MACRO(PrintType,            Print,               type::Type const*,       type_arg_)
OP_MACRO(PrintEnum,            Print,               EnumVal,                 print_enum_)
OP_MACRO(PrintFlags,           Print,               FlagsVal,                print_flags_)
OP_MACRO(PrintAddr,            Print,               ir::Addr,                addr_arg_)
OP_MACRO(PrintByteView,        Print,               std::string_view,        byte_view_arg_)
OP_MACRO(PrintInterface,       Print,               type::Interface const*,  intf_arg_)
OP_MACRO(AddInt8,              Add,                 int8_t,                   i8_args_)
OP_MACRO(AddInt16,             Add,                 int16_t,                  i16_args_)
OP_MACRO(AddInt32,             Add,                 int32_t,                  i32_args_)
OP_MACRO(AddInt64,             Add,                 int64_t,                  i64_args_)
OP_MACRO(AddNat8,              Add,                 uint8_t,                 u8_args_)
OP_MACRO(AddNat16,             Add,                 uint16_t,                 u16_args_)
OP_MACRO(AddNat32,             Add,                 uint32_t,                 u32_args_)
OP_MACRO(AddNat64,             Add,                 uint64_t,                 u64_args_)
OP_MACRO(AddFloat32,           Add,                 float,                   float32_args_)
OP_MACRO(AddFloat64,           Add,                 double,                  float64_args_)
OP_MACRO(SubInt8,              Sub,                 int8_t,                   i8_args_)
OP_MACRO(SubInt16,             Sub,                 int16_t,                  i16_args_)
OP_MACRO(SubInt32,             Sub,                 int32_t,                  i32_args_)
OP_MACRO(SubInt64,             Sub,                 int64_t,                  i64_args_)
OP_MACRO(SubNat8,              Sub,                 uint8_t,                 u8_args_)
OP_MACRO(SubNat16,             Sub,                 uint16_t,                u16_args_)
OP_MACRO(SubNat32,             Sub,                 uint32_t,                u32_args_)
OP_MACRO(SubNat64,             Sub,                 uint64_t,                u64_args_)
OP_MACRO(SubFloat32,           Sub,                 float,                   float32_args_)
OP_MACRO(SubFloat64,           Sub,                 double,                  float64_args_)
OP_MACRO(MulInt8,              Mul,                 int8_t,                  i8_args_)
OP_MACRO(MulInt16,             Mul,                 int16_t,                 i16_args_)
OP_MACRO(MulInt32,             Mul,                 int32_t,                 i32_args_)
OP_MACRO(MulInt64,             Mul,                 int64_t,                 i64_args_)
OP_MACRO(MulNat8,              Mul,                 uint8_t,                 u8_args_)
OP_MACRO(MulNat16,             Mul,                 uint16_t,                u16_args_)
OP_MACRO(MulNat32,             Mul,                 uint32_t,                u32_args_)
OP_MACRO(MulNat64,             Mul,                 uint64_t,                u64_args_)
OP_MACRO(MulFloat32,           Mul,                 float,                   float32_args_)
OP_MACRO(MulFloat64,           Mul,                 double,                  float64_args_)
OP_MACRO(DivInt8,              Div,                 int8_t,                  i8_args_)
OP_MACRO(DivInt16,             Div,                 int16_t,                 i16_args_)
OP_MACRO(DivInt32,             Div,                 int32_t,                 i32_args_)
OP_MACRO(DivInt64,             Div,                 int64_t,                 i64_args_)
OP_MACRO(DivNat8,              Div,                 uint8_t,                 u8_args_)
OP_MACRO(DivNat16,             Div,                 uint16_t,                u16_args_)
OP_MACRO(DivNat32,             Div,                 uint32_t,                u32_args_)
OP_MACRO(DivNat64,             Div,                 uint64_t,                u64_args_)
OP_MACRO(DivFloat32,           Div,                 float,                   float32_args_)
OP_MACRO(DivFloat64,           Div,                 double,                  float64_args_)
OP_MACRO(ModInt8,              Mod,                 int8_t,                  i8_args_)
OP_MACRO(ModInt16,             Mod,                 int16_t,                 i16_args_)
OP_MACRO(ModInt32,             Mod,                 int32_t,                 i32_args_)
OP_MACRO(ModInt64,             Mod,                 int64_t,                 i64_args_)
OP_MACRO(ModNat8,              Mod,                 uint8_t,                 u8_args_)
OP_MACRO(ModNat16,             Mod,                 uint16_t,                u16_args_)
OP_MACRO(ModNat32,             Mod,                 uint32_t,                u32_args_)
OP_MACRO(ModNat64,             Mod,                 uint64_t,                u64_args_)
OP_MACRO(LtInt8,               Lt,                  int8_t,                  i8_args_)
OP_MACRO(LtInt16,              Lt,                  int16_t,                 i16_args_)
OP_MACRO(LtInt32,              Lt,                  int32_t,                 i32_args_)
OP_MACRO(LtInt64,              Lt,                  int64_t,                 i64_args_)
OP_MACRO(LtNat8,               Lt,                  uint8_t,                 u8_args_)
OP_MACRO(LtNat16,              Lt,                  uint16_t,                u16_args_)
OP_MACRO(LtNat32,              Lt,                  uint32_t,                u32_args_)
OP_MACRO(LtNat64,              Lt,                  uint64_t,                u64_args_)
OP_MACRO(LtFloat32,            Lt,                  float,                   float32_args_)
OP_MACRO(LtFloat64,            Lt,                  double,                  float64_args_)
OP_MACRO(LtFlags,              Lt,                  FlagsVal,                flags_args_)
OP_MACRO(LeInt8,               Le,                  int8_t,                  i8_args_)
OP_MACRO(LeInt16,              Le,                  int16_t,                 i16_args_)
OP_MACRO(LeInt32,              Le,                  int32_t,                 i32_args_)
OP_MACRO(LeInt64,              Le,                  int64_t,                 i64_args_)
OP_MACRO(LeNat8,               Le,                  uint8_t,                 u8_args_)
OP_MACRO(LeNat16,              Le,                  uint16_t,                u16_args_)
OP_MACRO(LeNat32,              Le,                  uint32_t,                u32_args_)
OP_MACRO(LeNat64,              Le,                  uint64_t,                u64_args_)
OP_MACRO(LeFloat32,            Le,                  float,                   float32_args_)
OP_MACRO(LeFloat64,            Le,                  double,                  float64_args_)
OP_MACRO(LeFlags,              Le,                  FlagsVal,                flags_args_)
OP_MACRO(GtInt8,               Gt,                  int8_t,                  i8_args_)
OP_MACRO(GtInt16,              Gt,                  int16_t,                 i16_args_)
OP_MACRO(GtInt32,              Gt,                  int32_t,                 i32_args_)
OP_MACRO(GtInt64,              Gt,                  int64_t,                 i64_args_)
OP_MACRO(GtNat8,               Gt,                  uint8_t,                 u8_args_)
OP_MACRO(GtNat16,              Gt,                  uint16_t,                u16_args_)
OP_MACRO(GtNat32,              Gt,                  uint32_t,                u32_args_)
OP_MACRO(GtNat64,              Gt,                  uint64_t,                u64_args_)
OP_MACRO(GtFloat32,            Gt,                  float,                   float32_args_)
OP_MACRO(GtFloat64,            Gt,                  double,                  float64_args_)
OP_MACRO(GtFlags,              Gt,                  FlagsVal,                flags_args_)
OP_MACRO(GeInt8,               Ge,                  int8_t,                  i8_args_)
OP_MACRO(GeInt16,              Ge,                  int16_t,                 i16_args_)
OP_MACRO(GeInt32,              Ge,                  int32_t,                 i32_args_)
OP_MACRO(GeInt64,              Ge,                  int64_t,                 i64_args_)
OP_MACRO(GeNat8,               Ge,                  uint8_t,                 u8_args_)
OP_MACRO(GeNat16,              Ge,                  uint16_t,                u16_args_)
OP_MACRO(GeNat32,              Ge,                  uint32_t,                u32_args_)
OP_MACRO(GeNat64,              Ge,                  uint64_t,                u64_args_)
OP_MACRO(GeFloat32,            Ge,                  float,                   float32_args_)
OP_MACRO(GeFloat64,            Ge,                  double,                  float64_args_)
OP_MACRO(GeFlags,              Ge,                  FlagsVal,                flags_args_)
OP_MACRO(EqBool,               Eq,                  bool,                    bool_args_)
OP_MACRO(EqInt8,               Eq,                  int8_t,                  i8_args_)
OP_MACRO(EqInt16,              Eq,                  int16_t,                 i16_args_)
OP_MACRO(EqInt32,              Eq,                  int32_t,                 i32_args_)
OP_MACRO(EqInt64,              Eq,                  int64_t,                 i64_args_)
OP_MACRO(EqNat8,               Eq,                  uint8_t,                 u8_args_)
OP_MACRO(EqNat16,              Eq,                  uint16_t,                u16_args_)
OP_MACRO(EqNat32,              Eq,                  uint32_t,                u32_args_)
OP_MACRO(EqNat64,              Eq,                  uint64_t,                u64_args_)
OP_MACRO(EqFloat32,            Eq,                  float,                   float32_args_)
OP_MACRO(EqFloat64,            Eq,                  double,                  float64_args_)
OP_MACRO(EqType,               Eq,                  type::Type const*,       type_args_)
OP_MACRO(EqEnum,               Eq,                  EnumVal,                 enum_args_)
OP_MACRO(EqFlags,              Eq,                  FlagsVal,                flags_args_)
OP_MACRO(EqAddr,               Eq,                  ir::Addr,                addr_args_)
OP_MACRO(NeInt8,               Ne,                  int8_t,                  i8_args_)
OP_MACRO(NeInt16,              Ne,                  int16_t,                 i16_args_)
OP_MACRO(NeInt32,              Ne,                  int32_t,                 i32_args_)
OP_MACRO(NeInt64,              Ne,                  int64_t,                 i64_args_)
OP_MACRO(NeNat8,               Ne,                  uint8_t,                 u8_args_)
OP_MACRO(NeNat16,              Ne,                  uint16_t,                u16_args_)
OP_MACRO(NeNat32,              Ne,                  uint32_t,                u32_args_)
OP_MACRO(NeNat64,              Ne,                  uint64_t,                u64_args_)
OP_MACRO(NeFloat32,            Ne,                  float,                   float32_args_)
OP_MACRO(NeFloat64,            Ne,                  double,                  float64_args_)
OP_MACRO(NeType,               Ne,                  type::Type const*,       type_args_)
OP_MACRO(NeEnum,               Ne,                  EnumVal,                 enum_args_)
OP_MACRO(NeFlags,              Ne,                  FlagsVal,                flags_args_)
OP_MACRO(NeAddr,               Ne,                  ir::Addr,                addr_args_)
OP_MACRO(XorBool,              Xor,                 bool,                    bool_args_)
OP_MACRO(XorFlags,             Xor,                 FlagsVal,                flags_args_)
OP_MACRO(OrFlags,              Or,                  FlagsVal,                flags_args_)
OP_MACRO(AndFlags,             And,                 FlagsVal,                flags_args_)
OP_MACRO(CreateStruct,         CreateStruct,        ::Scope const *,         scope_)
OP_MACRO(CreateStructField,    CreateStructField,   void,                    create_struct_field_)
OP_MACRO(SetStructFieldName,   SetStructFieldName,  void,                    set_struct_field_name_)
OP_MACRO(AddHashtagToField,    AddHashtagToField,   void,                    add_hashtag_)
OP_MACRO(AddHashtagToStruct,   AddHashtagToStruct,  void,                    add_hashtag_)
OP_MACRO(FinalizeStruct,       Finalize,            type::Struct const*,     reg_)
OP_MACRO(CreateEnum,           CreateEnum,          void,                    mod_)
OP_MACRO(AddEnumerator,        AddEnumerator,       void,                    add_enumerator_)
OP_MACRO(SetEnumerator,        SetEnumerator,       void,                    set_enumerator_)
OP_MACRO(FinalizeEnum,         FinalizeEnum,        void,                    reg_)
OP_MACRO(CreateFlags,          CreateFlags,         void,                    mod_)
OP_MACRO(AddFlag,              AddFlags,            void,                    add_enumerator_)
OP_MACRO(SetFlag,              SetFlags,            void,                    set_enumerator_)
OP_MACRO(FinalizeFlags,        FinalizeFlags,       void,                    reg_)
OP_MACRO(CreateInterface,      CreateInterface,     ::Scope const *,         scope_)
OP_MACRO(FinalizeInterface,    FinalizeInterface,   void,                    reg_)
OP_MACRO(DebugIr,              DebugIr,             void,                    empty_)
OP_MACRO(Alloca,               Alloca,              type::Type const*,       type_)
OP_MACRO(PtrIncr,              PtrIncr,             void,                    ptr_incr_)
OP_MACRO(Field,                Field,               void,                    field_)
OP_MACRO(BufPtr,               BufPtr,              void,                    reg_)
OP_MACRO(Ptr,                  Ptr,                 void,                    reg_)
OP_MACRO(Arrow,                Arrow,               type::Type const*,       type_args_)
OP_MACRO(Array,                Array,               void,                    array_)
OP_MACRO(CreateTuple,          Create,              type::Tuple const*,      empty_)
OP_MACRO(AppendToTuple,        Append,              type::Tuple const*,      store_type_)
OP_MACRO(FinalizeTuple,        Finalize,            type::Tuple const*,      reg_)
OP_MACRO(CreateVariant,        Create,              type::Variant const*,    empty_)
OP_MACRO(AppendToVariant,      Append,              type::Variant const*,    store_type_)
OP_MACRO(FinalizeVariant,      Finalize,            type::Variant const*,    reg_)
OP_MACRO(CreateBlockSeq,       Create,              BlockSequence,           empty_)
OP_MACRO(AppendToBlockSeq,     Append,              BlockSequence,           store_block_)
OP_MACRO(FinalizeBlockSeq,     Finalize,            BlockSequence,           reg_)
OP_MACRO(CondJump,             Jump,                bool,                    cond_jump_)
OP_MACRO(UncondJump,           Jump,                void,                    block_)
OP_MACRO(ReturnJump,           Jump,                int32_t, /* any tag */   empty_)
OP_MACRO(BlockSeqJump,         Jump,                BlockSequence,           block_seq_jump_)
OP_MACRO(VariantType,          VariantType,         void,                    addr_arg_)
OP_MACRO(VariantValue,         VariantValue,        void,                    addr_arg_)
OP_MACRO(Call,                 Call,                void,                    call_)
OP_MACRO(CastToInt8,           Cast,                int8_t,                  typed_reg_)
OP_MACRO(CastToNat8,           Cast,                uint8_t,                 typed_reg_)
OP_MACRO(CastToInt16,          Cast,                int16_t,                 typed_reg_)
OP_MACRO(CastToNat16,          Cast,                uint16_t,                typed_reg_)
OP_MACRO(CastToInt32,          Cast,                int32_t,                 typed_reg_)
OP_MACRO(CastToNat32,          Cast,                uint32_t,                typed_reg_)
OP_MACRO(CastToInt64,          Cast,                int64_t,                 typed_reg_)
OP_MACRO(CastToNat64,          Cast,                uint64_t,                typed_reg_)
OP_MACRO(CastToFloat32,        Cast,                float,                   typed_reg_)
OP_MACRO(CastToFloat64,        Cast,                double,                  typed_reg_)
OP_MACRO(CastToEnum,           Cast,                type::Enum const *,      reg_)
OP_MACRO(CastToFlags,          Cast,                type::Flags const *,     reg_)
OP_MACRO(CastPtr,              CastPtr,             type::Type const*,       typed_reg_)
OP_MACRO(PhiBool,              Phi,                 bool,                    phi_bool_)
OP_MACRO(PhiInt8,              Phi,                 int8_t,                  phi_i8_)
OP_MACRO(PhiInt16,             Phi,                 int16_t,                 phi_i16_)
OP_MACRO(PhiInt32,             Phi,                 int32_t,                 phi_i32_)
OP_MACRO(PhiInt64,             Phi,                 int64_t,                 phi_i64_)
OP_MACRO(PhiNat8,              Phi,                 uint8_t,                 phi_u8_)
OP_MACRO(PhiNat16,             Phi,                 uint16_t,                phi_u16_)
OP_MACRO(PhiNat32,             Phi,                 uint32_t,                phi_u32_)
OP_MACRO(PhiNat64,             Phi,                 uint64_t,                phi_u64_)
OP_MACRO(PhiFloat32,           Phi,                 float,                   phi_float32_)
OP_MACRO(PhiFloat64,           Phi,                 double,                  phi_float64_)
OP_MACRO(PhiType,              Phi,                 type::Type const*,       phi_type_)
OP_MACRO(PhiBlock,             Phi,                 BlockSequence,           phi_block_)
OP_MACRO(PhiAddr,              Phi,                 ir::Addr,                phi_addr_)
OP_MACRO(PhiEnum,              Phi,                 ir::EnumVal,             phi_enum_)
OP_MACRO(PhiFlags,             Phi,                 ir::FlagsVal,            phi_flags_)
OP_MACRO(BlockSeqContains,     BlockSeqContains,    void,                    block_seq_contains_)
OP_MACRO(GetRet,               GetRet,              void,                    get_ret_)
OP_MACRO(SetRetBool,           SetRet,              bool,                    set_ret_bool_)
OP_MACRO(SetRetInt8,           SetRet,              int8_t,                  set_ret_i8_)
OP_MACRO(SetRetInt16,          SetRet,              int16_t,                 set_ret_i16_)
OP_MACRO(SetRetInt32,          SetRet,              int32_t,                 set_ret_i32_)
OP_MACRO(SetRetInt64,          SetRet,              int64_t,                 set_ret_i64_)
OP_MACRO(SetRetNat8,           SetRet,              uint8_t,                 set_ret_u8_)
OP_MACRO(SetRetNat16,          SetRet,              uint16_t,                set_ret_u16_)
OP_MACRO(SetRetNat32,          SetRet,              uint32_t,                set_ret_u32_)
OP_MACRO(SetRetNat64,          SetRet,              uint64_t,                set_ret_u64_)
OP_MACRO(SetRetFloat32,        SetRet,              float,                   set_ret_float32_)
OP_MACRO(SetRetFloat64,        SetRet,              double,                  set_ret_float64_)
OP_MACRO(SetRetType,           SetRet,              type::Type const*,       set_ret_type_)
OP_MACRO(SetRetEnum,           SetRet,              EnumVal,                 set_ret_enum_)
OP_MACRO(SetRetFlags,          SetRet,              FlagsVal,                set_ret_flags_)
OP_MACRO(SetRetByteView,       SetRet,              std::string_view,        set_ret_byte_view_)
OP_MACRO(SetRetAddr,           SetRet,              ir::Addr,                set_ret_addr_)
OP_MACRO(SetRetFunc,           SetRet,              AnyFunc,                 set_ret_func_)
OP_MACRO(SetRetScope,          SetRet,              ast::ScopeLiteral*,      set_ret_scope_)
OP_MACRO(SetRetGeneric,        SetRet,              ast::FunctionLiteral  *, set_ret_generic_)
OP_MACRO(SetRetModule,         SetRet,              Module const*,           set_ret_module_)
OP_MACRO(SetRetBlock,          SetRet,              BlockSequence,           set_ret_block_)
OP_MACRO(SetRetInterface,      SetRet,              type::Interface const*,  set_ret_intf_)
OP_MACRO(ArgumentCache,        ArgumentCache,       ast::StructLiteral *,    sl_)
OP_MACRO(NewOpaqueType,        NewOpaqueType,       void,                    reg_)
OP_MACRO(LoadSymbol,           LoadSymbol,          void,                    load_sym_)
OP_MACRO(Init,                 Init,                void,                    special1_)
OP_MACRO(Destroy,              Destroy,             void,                    special1_)
OP_MACRO(Move,                 Move,                void,                    special2_)
OP_MACRO(Copy,                 Copy,                void,                    special2_)

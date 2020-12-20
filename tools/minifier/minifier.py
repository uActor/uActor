from antlr4 import *
from .LuaLexer import LuaLexer
from .LuaListener import LuaListener
from .LuaParser import LuaParser


# Framework generated from Lua.g4 by ANTLR 4.
class LuaMinifier(ParseTreeVisitor):

    # Visit a parse tree produced by LuaParser#chunk.
    def visitChunk(self, ctx:LuaParser.ChunkContext):
        self.vars_mapping = {}
        self.var_base = ""
        self.next_var = "a"
        return self.visitBlock(ctx.block())


    # Visit a parse tree produced by LuaParser#block.
    def visitBlock(self, ctx:LuaParser.BlockContext):
        return "\n".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#stat.
    def visitStat(self, ctx:LuaParser.StatContext):
        cc = ctx.getChildCount()
        if cc == 3:
            if str(self.visit(ctx.getChild(1))) == "=":
                return "".join([self.visit(c) for c in ctx.getChildren()])
            elif str(self.visit(ctx.getChild(0))) == "do":
                return f"do\n{str(self.visit(ctx.getChild(1)))}\nend"
        elif cc == 4:
            if str(self.visit(ctx.getChild(0))) == "while":
               return f"while {str(self.visit(ctx.getChild(1)))} do\n{str(self.visit(ctx.getChild(3)))}\nend"
            elif str(self.visit(ctx.getChild(0))) == "repeat":
               return f"repeat\n{str(self.visit(ctx.getChild(1)))}\nuntil {str(self.visit(ctx.getChild(3)))}" 
        
        if str(self.visit(ctx.getChild(0))) == "if":
            out = f"if {str(self.visit(ctx.getChild(1)))} then\n{str(self.visit(ctx.getChild(3)))}\n"
            index = 4
            while (index < cc-1 and str(self.visit(ctx.getChild(index))) == "elseif"):
                out += f"elseif {str(self.visit(ctx.getChild(index+1)))} then\n{str(self.visit(ctx.getChild(index+3)))}\n"
                index +=4
            if(index < cc-1):
                out += f"else\n{str(self.visit(ctx.getChild(index+1)))}\n"
            out += "end"
            return out


        if str(self.visit(ctx.getChild(0))) == "for":
            if str(self.visit(ctx.getChild(2))) == "=":
                out = f"for {self.shorten_name(self.visit(ctx.getChild(1)))}={self.visit(ctx.getChild(3))},{self.visit(ctx.getChild(5))}"
                index = 7
                if(cc > 9):
                    out += f",{self.visit(ctx.getChild(index))}"
                    index += 2
                return out+f" do {self.visit(ctx.getChild(index))}\nend"
            elif str(self.visit(ctx.getChild(2))) == "in":
                return f"for {self.visit(ctx.getChild(1))} in {self.visit(ctx.getChild(3))} do\n{self.visit(ctx.getChild(5))}\nend"
        
        if str(self.visit(ctx.getChild(0))) == "function":
            return f"function {self.visit(ctx.getChild(1))}{self.visit(ctx.getChild(2))}"

        if str(self.visit(ctx.getChild(0))) == "local" and str(self.visit(ctx.getChild(1))) == "function":
            return f"local function {self.shorten_name(self.visit(ctx.getChild(1)))}{self.visit(ctx.getChild(2))}"

        return " ".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#attnamelist.
    def visitAttnamelist(self, ctx:LuaParser.AttnamelistContext):
        out = ""
        for i in range(0, ctx.getChildCount()):
            if type(ctx.getChild(i)) != LuaParser.AttribContext and self.visit(ctx.getChild(i)) != ",":
                out += self.shorten_name(self.visit(ctx.getChild(i)))
            else:
                out += self.visit(ctx.getChild(i))
        return out


    # Visit a parse tree produced by LuaParser#attrib.
    def visitAttrib(self, ctx:LuaParser.AttribContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#retstat.
    def visitRetstat(self, ctx:LuaParser.RetstatContext):
        if ctx.getChildCount() == 1:
            return "return"
        else:
            return "return " + str(self.visit(ctx.getChild(1)))


    # Visit a parse tree produced by LuaParser#label.
    def visitLabel(self, ctx:LuaParser.LabelContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#funcname.
    def visitFuncname(self, ctx:LuaParser.FuncnameContext):
        if(ctx.getChildCount() == 1):
            return self.shorten_name(self.visit(ctx.getChild(0)))
        else:
            return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#varlist.
    def visitVarlist(self, ctx:LuaParser.VarlistContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#namelist.
    def visitNamelist(self, ctx:LuaParser.NamelistContext):
        out = ""
        for i in range(0, ctx.getChildCount()):
            if i%2 == 0:
                out += self.shorten_name(self.visit(ctx.getChild(i)))
            else:
                out += self.visit(ctx.getChild(i))
        return out


    # Visit a parse tree produced by LuaParser#explist.
    def visitExplist(self, ctx:LuaParser.ExplistContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#exp.
    def visitExp(self, ctx:LuaParser.ExpContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#prefixexp.
    def visitPrefixexp(self, ctx:LuaParser.PrefixexpContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#functioncall.
    def visitFunctioncall(self, ctx:LuaParser.FunctioncallContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#varOrExp.
    def visitVarOrExp(self, ctx:LuaParser.VarOrExpContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#var.
    def visitVar(self, ctx:LuaParser.VarContext):

        out = self.visit(ctx.getChild(0))
        out = self.shorten_name(out)

        for i in range(1, ctx.getChildCount()):
            out+=self.visit(ctx.getChild(i))
        return out


    # Visit a parse tree produced by LuaParser#varSuffix.
    def visitVarSuffix(self, ctx:LuaParser.VarSuffixContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#nameAndArgs.
    def visitNameAndArgs(self, ctx:LuaParser.NameAndArgsContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#args.
    def visitArgs(self, ctx:LuaParser.ArgsContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#functiondef.
    def visitFunctiondef(self, ctx:LuaParser.FunctiondefContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#funcbody.
    def visitFuncbody(self, ctx:LuaParser.FuncbodyContext):
        return f"({self.visit(ctx.getChild(1)) if(ctx.getChildCount() == 5) else ''})\n{self.visit(ctx.getChild(3) if ctx.getChildCount() == 5 else ctx.getChild(2))}\nend"


    # Visit a parse tree produced by LuaParser#parlist.
    def visitParlist(self, ctx:LuaParser.ParlistContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#tableconstructor.
    def visitTableconstructor(self, ctx:LuaParser.TableconstructorContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#fieldlist.
    def visitFieldlist(self, ctx:LuaParser.FieldlistContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#field.
    def visitField(self, ctx:LuaParser.FieldContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#fieldsep.
    def visitFieldsep(self, ctx:LuaParser.FieldsepContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#operatorOr.
    def visitOperatorOr(self, ctx:LuaParser.OperatorOrContext):
        return " or "


    # Visit a parse tree produced by LuaParser#operatorAnd.
    def visitOperatorAnd(self, ctx:LuaParser.OperatorAndContext):
        return " and "


    # Visit a parse tree produced by LuaParser#operatorComparison.
    def visitOperatorComparison(self, ctx:LuaParser.OperatorComparisonContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#operatorStrcat.
    def visitOperatorStrcat(self, ctx:LuaParser.OperatorStrcatContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#operatorAddSub.
    def visitOperatorAddSub(self, ctx:LuaParser.OperatorAddSubContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#operatorMulDivMod.
    def visitOperatorMulDivMod(self, ctx:LuaParser.OperatorMulDivModContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#operatorBitwise.
    def visitOperatorBitwise(self, ctx:LuaParser.OperatorBitwiseContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#operatorUnary.
    def visitOperatorUnary(self, ctx:LuaParser.OperatorUnaryContext):
        child = str(ctx.getChild(0))
        if(child == "not"):
            child = "not "
        return child


    # Visit a parse tree produced by LuaParser#operatorPower.
    def visitOperatorPower(self, ctx:LuaParser.OperatorPowerContext):
        return "^"


    # Visit a parse tree produced by LuaParser#number.
    def visitNumber(self, ctx:LuaParser.NumberContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])


    # Visit a parse tree produced by LuaParser#string.
    def visitString(self, ctx:LuaParser.StringContext):
        return "".join([self.visit(c) for c in ctx.getChildren()])

    def visitTerminal(self, terminal):
        if(terminal != None):
            return str(terminal)

    def shorten_name(self, name):

        known_keys = {"receive" , "node_id", "actor_type", "instance_id", "publish", "delayed_publish", "deferred_block_for",
        "subscribe", "unsubscribe", "encode_base64", "decode_base64", "testbed_log_integer", "testbed_log_double",
        "testbed_log_string", "testbed_start_timekeeping", "testbed_stop_timekeeping", "calculate_time_diff", "connection_traffic",
        "testbed_stop_timekeeping_inner", "unix_timestamp", "assert", "collectgarbage", "error", "ipairs", "next", "pairs", "print", "select",
        "tonumber", "tostring", "type", "math", "string", "Publication", "now"}

        if name not in known_keys:
            if name not in self.vars_mapping:
                value = self.var_base + self.next_var
                self.vars_mapping[name] = value
                if(self.next_var == "z"):
                    self.var_base += "a"
                    self.next_var = "a"
                else:
                    self.next_var = chr(ord(self.next_var) + 1)
            return self.vars_mapping[name]
        else:
            return name


def minify(code):
  lexer = LuaLexer(InputStream(code))
  stream = CommonTokenStream(lexer)
  parser = LuaParser(stream)
  root = parser.chunk()
  v = LuaMinifier()
  res = v.visitChunk(root)
  #print(res)
  return res
  

def main():
    lexer = LuaLexer(FileStream("../../evaluation/smart_office/environment_logger.lua"))
    stream = CommonTokenStream(lexer)
    parser = LuaParser(stream)
    root = parser.chunk()
    v = LuaMinifier()
    print(v.visitChunk(root))

if __name__ == '__main__':
    main()
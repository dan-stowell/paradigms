// p4dump.go: problem 4's instrumentation of this copy of fc (paradigms ref/fc; not in simd-lang).
//
// With FC_P4DUMP=<file>, every function the tree walker emits is written to <file> as it is finished: its
// body (the bytes the walk wrote, after every jump and frame size is patched, before the prologue is put in
// front), the body's relocations, the holes (where the encoder wrote a value that depends on an operand:
// a displacement, an immediate, a jump's rel32, the frame size) and the expression nodes, each the interval
// of body bytes written while the walker was inside it. Nothing here changes what is emitted: without
// FC_P4DUMP no state is made, and with it the emitter only appends to lists on the side (ref/p4/NOTES.md
// checks that the built program is byte-identical either way).
package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"go/ast"
	"go/types"
	"os"
	"strings"
	"sync"
)

const (
	p4Frame     = 1 // a displacement from BP: a local, a temporary, a parameter's home
	p4Out       = 2 // a displacement from SP: an outgoing argument
	p4Disp      = 3 // a displacement from another base register: a field offset, an element offset
	p4Imm       = 4 // an immediate
	p4FrameSize = 5 // the frame size (LEAQ n(SP), BP), patched in when the function is finished
	p4Label     = 6 // a jump's rel32
)

type p4hole struct {
	pc   int32
	w    uint8
	kind uint8
}

type p4node struct {
	start, end, parent int32
	e                  ast.Expr
	shape, mode        string
}

type p4state struct {
	holes []p4hole
	nodes []p4node
	stack []int32
}

var (
	p4Path = os.Getenv("FC_P4DUMP")
	p4Mu   sync.Mutex
	p4File *os.File
)

func p4new() *p4state {
	if p4Path == "" {
		return nil
	}
	return &p4state{}
}

func (s *p4state) hole(pc int, w int, kind uint8) {
	if s != nil {
		s.holes = append(s.holes, p4hole{int32(pc), uint8(w), kind})
	}
}

// p4interior: is e a node of SHAPES.md's count (explore/wholeprog), not a leaf (a variable, a constant, a
// type, pkg.Name) or a paren?
func p4interior(e ast.Expr) bool {
	tv := tvOf(e)
	if tv.Value != nil || tv.IsType() || tv.IsNil() {
		return false
	}
	switch x := e.(type) {
	case nil, *ast.Ident, *ast.BasicLit, *ast.ParenExpr, *ast.KeyValueExpr, *ast.Ellipsis:
		return false
	case *ast.SelectorExpr:
		if id, ok := x.X.(*ast.Ident); ok {
			if _, ok := useOf(id).(*types.PkgName); ok {
				return false
			}
		}
	}
	return true
}

// p4enter / p4leave bracket the walker's work on one expression node. Entering the node the walker is
// already inside (expr -> binary on the same node) is the same node; the outermost entry names the mode.
func (g *gen) p4enter(e ast.Expr, mode string) int32 {
	s := g.p4
	if s == nil {
		return -1
	}
	e = ast.Unparen(e)
	if !p4interior(e) {
		return -1
	}
	parent := int32(-1)
	if n := len(s.stack); n > 0 {
		parent = s.stack[n-1]
		if s.nodes[parent].e == e {
			return -1
		}
	}
	i := int32(len(s.nodes))
	s.nodes = append(s.nodes, p4node{start: int32(g.a.pc()), end: -1, parent: parent, e: e, shape: g.p4shape(e), mode: mode})
	s.stack = append(s.stack, i)
	return i
}

func (g *gen) p4leave(i int32) {
	if i < 0 {
		return
	}
	s := g.p4
	s.nodes[i].end = int32(g.a.pc())
	if n := len(s.stack); n > 0 && s.stack[n-1] == i {
		s.stack = s.stack[:n-1]
	} else {
		panic("p4leave: not the innermost node")
	}
}

// p4trunc: place() took back a jump to the next instruction: the body is at bytes now.
func (g *gen) p4trunc(at int) {
	s := g.p4
	if s == nil {
		return
	}
	a := int32(at)
	for len(s.holes) > 0 && s.holes[len(s.holes)-1].pc >= a {
		s.holes = s.holes[:len(s.holes)-1]
	}
	for i := range s.nodes {
		n := &s.nodes[i]
		if n.start > a {
			n.start = a
		}
		if n.end > a {
			n.end = a
		}
	}
}

// ---- typed shape: explore/wholeprog's node description (operator, operand descriptions, result class)

func p4class(c *compiler, t types.Type) (s string) {
	if t == nil {
		return "-"
	}
	switch u := t.Underlying().(type) {
	case *types.Basic:
		switch {
		case u.Kind() == types.UntypedNil:
			return "nil"
		case u.Info()&types.IsBoolean != 0:
			return "bool"
		case u.Info()&types.IsString != 0:
			return "str"
		case u.Info()&types.IsFloat != 0:
			if u.Kind() == types.Float32 {
				return "f32"
			}
			return "f64"
		case u.Info()&types.IsComplex != 0:
			return "cplx"
		case u.Info()&types.IsInteger != 0:
			s := "i"
			if u.Info()&types.IsUnsigned != 0 {
				s = "u"
			}
			switch u.Kind() {
			case types.Int8, types.Uint8:
				return s + "8"
			case types.Int16, types.Uint16:
				return s + "16"
			case types.Int32, types.Uint32:
				return s + "32"
			default:
				return s + "64"
			}
		case u.Kind() == types.UnsafePointer:
			return "ptr"
		}
		return "basic"
	case *types.Pointer:
		return "ptr"
	case *types.Slice:
		return "slice"
	case *types.Map:
		return "map"
	case *types.Chan:
		return "chan"
	case *types.Signature:
		return "func"
	case *types.Interface:
		if _, ok := t.(*types.TypeParam); ok {
			return "tparam"
		}
		if u.NumMethods() == 0 {
			return "eface"
		}
		return "iface"
	case *types.Struct:
		sz := p4sizeof(c, t)
		switch {
		case sz < 0:
			return "stgen"
		case sz <= 8:
			return "st8"
		case sz <= 16:
			return "st16"
		case sz <= 64:
			return "st64"
		}
		return "stbig"
	case *types.Array:
		sz := p4sizeof(c, t)
		if sz < 0 {
			return "argen"
		}
		if sz <= 16 {
			return "ar16"
		}
		return "arbig"
	case *types.Tuple:
		return fmt.Sprintf("tuple%d", u.Len())
	}
	return "other"
}

func p4sizeof(c *compiler, t types.Type) (sz int64) {
	defer func() {
		if recover() != nil {
			sz = -1
		}
	}()
	return c.sizeof(t)
}

func (g *gen) p4type(e ast.Expr) types.Type {
	tv := tvOf(e)
	if tv.Type == nil {
		return nil
	}
	return g.subst(tv.Type)
}

func p4leaf(id *ast.Ident) string {
	obj := useOf(id)
	if obj == nil {
		obj = defOf(id)
	}
	switch o := obj.(type) {
	case *types.Var:
		if o.IsField() {
			return "field"
		}
		if o.Pkg() != nil && o.Parent() == o.Pkg().Scope() {
			return "global"
		}
		return "local"
	case *types.Const:
		return "const"
	case *types.Func:
		return "funcname"
	case *types.Nil:
		return "nil"
	case *types.TypeName:
		return "type"
	case *types.Builtin:
		return "builtin:" + o.Name()
	case *types.PkgName:
		return "pkg"
	}
	return "ident"
}

// p4short: what a parent node sees of operand e (wholeprog's short description).
func (g *gen) p4short(e ast.Expr) string {
	if e == nil {
		return "_"
	}
	e = ast.Unparen(e)
	tv := tvOf(e)
	c := p4class(g.c, g.p4type(e))
	if tv.Value != nil {
		return "K:" + c
	}
	if tv.IsType() {
		return "T"
	}
	switch x := e.(type) {
	case *ast.Ident:
		return p4leaf(x) + ":" + c
	case *ast.BasicLit:
		return "K:" + c
	case *ast.SelectorExpr:
		if id, ok := x.X.(*ast.Ident); ok {
			if _, ok := useOf(id).(*types.PkgName); ok {
				return p4leaf(x.Sel) + ":" + c
			}
		}
	}
	return "t:" + c
}

func (g *gen) p4shape(e ast.Expr) (s string) {
	defer func() {
		if recover() != nil {
			s = fmt.Sprintf("%T ?", e)
		}
	}()
	var b strings.Builder
	op := func(name string, operands ...ast.Expr) {
		b.WriteString(name)
		for _, o := range operands {
			b.WriteByte(' ')
			b.WriteString(g.p4short(o))
		}
	}
	switch x := e.(type) {
	case *ast.BinaryExpr:
		op("bin"+x.Op.String(), x.X, x.Y)
	case *ast.UnaryExpr:
		op("un"+x.Op.String(), x.X)
	case *ast.StarExpr:
		op("deref", x.X)
	case *ast.SelectorExpr:
		op("sel."+p4leaf(x.Sel), x.X)
	case *ast.IndexExpr:
		op("index", x.X, x.Index)
	case *ast.IndexListExpr:
		b.WriteString("inst")
	case *ast.SliceExpr:
		op("slice", x.X, x.Low, x.High, x.Max)
	case *ast.TypeAssertExpr:
		op("assert", x.X)
	case *ast.CallExpr:
		if tvOf(x.Fun).IsType() {
			op("conv", x.Args...)
			break
		}
		callee := g.p4short(x.Fun)
		if i := strings.IndexByte(callee, ':'); i > 0 && !strings.HasPrefix(callee, "builtin") {
			callee = callee[:i]
		}
		op("call "+callee, x.Args...)
	case *ast.CompositeLit:
		fmt.Fprintf(&b, "lit n=%d", min(len(x.Elts), 8))
	case *ast.FuncLit:
		b.WriteString("closure")
	default:
		fmt.Fprintf(&b, "%T", e)
	}
	b.WriteString(" ->")
	b.WriteString(p4class(g.c, g.p4type(e)))
	return b.String()
}

// ---- the dump

func p4str(w *bytes.Buffer, s string) {
	binary.Write(w, binary.LittleEndian, uint32(len(s)))
	w.WriteString(s)
}

// p4dump writes one function: called by finish with the body as it goes into the object.
func (g *gen) p4dump(name string, body *asm) {
	s := g.p4
	if s == nil {
		return
	}
	le := binary.LittleEndian
	var w bytes.Buffer
	w.WriteByte('F')
	if g.out.sym != nil && g.out.sym.ABI() != 0 {
		name += fmt.Sprintf("<%d>", g.out.sym.ABI())
	}
	p4str(&w, name)
	binary.Write(&w, le, uint32(len(body.b)))
	w.Write(body.b)
	binary.Write(&w, le, uint32(len(body.relocs)))
	for _, r := range body.relocs {
		binary.Write(&w, le, r.Off)
		w.WriteByte(r.Siz)
		binary.Write(&w, le, uint16(r.Type))
		binary.Write(&w, le, r.Add)
		sym := ""
		if r.Sym != nil {
			sym = r.Sym.Name
			if r.Sym.ABI() != 0 {
				sym += fmt.Sprintf("<%d>", r.Sym.ABI())
			}
		}
		p4str(&w, sym)
	}
	binary.Write(&w, le, uint32(len(s.holes)))
	for _, h := range s.holes {
		binary.Write(&w, le, h.pc)
		w.WriteByte(h.w)
		w.WriteByte(h.kind)
	}
	binary.Write(&w, le, uint32(len(s.nodes)))
	for _, n := range s.nodes {
		if n.end < 0 {
			panic("p4dump: a node was not left")
		}
		binary.Write(&w, le, n.start)
		binary.Write(&w, le, n.end)
		binary.Write(&w, le, n.parent)
		p4str(&w, n.shape)
		p4str(&w, n.mode)
	}
	p4Mu.Lock()
	defer p4Mu.Unlock()
	if p4File == nil {
		f, err := os.Create(p4Path)
		if err != nil {
			panic(err)
		}
		p4File = f
	}
	if _, err := p4File.Write(w.Bytes()); err != nil {
		panic(err)
	}
}

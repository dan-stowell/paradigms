// mkflat: problem 4's flat files and oracle from the dump of ref/fc's instrumented fc (FC_P4DUMP).
//
//	mkflat <dump.bin> <out dir>      ->  <out>/{meta.txt, *.u8 ... , oracle/*}, stats on stdout
//
// See ref/p4/README.md for what each file says.
package main

import (
	"bufio"
	"encoding/binary"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

const (
	kFrame, kOut, kDisp, kImm, kFrameSize, kLabel, kSym = 1, 2, 3, 4, 5, 6, 7
	encInt, encRel32, encReloc                          = 1, 2, 3
)

type reloc struct {
	off  int32
	siz  uint8
	typ  uint16
	add  int64
	sym  string
	used bool
}
type hole struct {
	pc       int32
	w, kind  uint8
	typ      uint16 // reloc type
	val, aux int64
	sym      string
	isReloc  bool
}
type node struct {
	start, end, parent int32
	shape, mode        string
	kids               []int32
}
type fn struct {
	name   string
	body   []byte
	relocs []reloc
	holes  []hole
	nodes  []node
}

// ---- reading the dump

type rd struct {
	b []byte
	p int
}

func (r *rd) u8() uint8   { v := r.b[r.p]; r.p++; return v }
func (r *rd) u16() uint16 { v := binary.LittleEndian.Uint16(r.b[r.p:]); r.p += 2; return v }
func (r *rd) u32() uint32 { v := binary.LittleEndian.Uint32(r.b[r.p:]); r.p += 4; return v }
func (r *rd) i64() int64  { v := binary.LittleEndian.Uint64(r.b[r.p:]); r.p += 8; return int64(v) }
func (r *rd) bytes(n int) []byte {
	v := r.b[r.p : r.p+n]
	r.p += n
	return v
}
func (r *rd) str() string { return string(r.bytes(int(r.u32()))) }

func readDump(path string) []*fn {
	data, err := os.ReadFile(path)
	check(err)
	r := &rd{b: data}
	var fns []*fn
	for r.p < len(data) {
		if r.u8() != 'F' {
			panic("bad dump")
		}
		f := &fn{name: r.str()}
		f.body = r.bytes(int(r.u32()))
		nr := int(r.u32())
		for i := 0; i < nr; i++ {
			var x reloc
			x.off = int32(r.u32())
			x.siz = r.u8()
			x.typ = r.u16()
			x.add = r.i64()
			x.sym = r.str()
			f.relocs = append(f.relocs, x)
		}
		nh := int(r.u32())
		for i := 0; i < nh; i++ {
			var h hole
			h.pc = int32(r.u32())
			h.w = r.u8()
			h.kind = r.u8()
			f.holes = append(f.holes, h)
		}
		nn := int(r.u32())
		for i := 0; i < nn; i++ {
			var n node
			n.start = int32(r.u32())
			n.end = int32(r.u32())
			n.parent = int32(r.u32())
			n.shape = r.str()
			n.mode = r.str()
			f.nodes = append(f.nodes, n)
		}
		fns = append(fns, f)
	}
	return fns
}

func check(err error) {
	if err != nil {
		panic(err)
	}
}

// ---- interning

type intern struct {
	ids  map[string]uint32
	list []string
}

func newIntern(first string) *intern {
	t := &intern{ids: map[string]uint32{}}
	t.id(first)
	return t
}
func (t *intern) id(s string) uint32 {
	if i, ok := t.ids[s]; ok {
		return i
	}
	i := uint32(len(t.list))
	t.ids[s] = i
	t.list = append(t.list, s)
	return i
}

// ---- output

type out struct{ dir string }

func (o out) write(name string, v any) {
	f, err := os.Create(filepath.Join(o.dir, name))
	check(err)
	w := bufio.NewWriterSize(f, 1<<20)
	check(binary.Write(w, binary.LittleEndian, v))
	check(w.Flush())
	check(f.Close())
}
func (o out) strings(name string, l []string) {
	off := make([]uint64, 0, len(l)+1)
	var bin []byte
	off = append(off, 0)
	for _, s := range l {
		bin = append(bin, s...)
		off = append(off, uint64(len(bin)))
	}
	o.write(name+".off.u64", off)
	o.write(name+".bin", bin)
}

type item struct {
	kind  uint8 // 0 glue, 1 a segment of a node
	ref   uint32
	seg   uint32
	start int32 // in the function body
	len   int32
}

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintln(os.Stderr, "usage: mkflat dump.bin outdir")
		os.Exit(2)
	}
	fns := readDump(os.Args[1])
	sort.SliceStable(fns, func(i, j int) bool { return fns[i].name < fns[j].name })
	for i := 1; i < len(fns); i++ {
		if fns[i].name == fns[i-1].name {
			panic("function emitted twice: " + fns[i].name)
		}
	}
	o := out{os.Args[2]}
	check(os.MkdirAll(filepath.Join(o.dir, "oracle"), 0o755))

	syms := newIntern("")
	shapes := newIntern("")
	modes := newIntern("")
	tmplIDs := map[string]uint32{}

	var (
		fnItem, fnMarkOff                               = []uint32{0}, []uint32{0}
		markOff                                         []uint32
		markType                                        []uint16
		markSym                                         []uint32
		markAdd                                         []int64
		itemKind                                        []uint8
		itemRef, itemSeg                                []uint32
		glueOff                                         = []uint64{0}
		glueBin                                         []byte
		glueRelOff                                      = []uint32{0}
		grelOff                                         []uint32
		grelSiz                                         []uint8
		grelType                                        []uint16
		grelSym                                         []uint32
		grelAdd                                         []int64
		nodeTmpl, nodeShape, nodeParent, nodeFn         []uint32
		nodeMode                                        []uint8
		nodeOp                                          = []uint32{0}
		opVal, opAux                                    []int64
		tmplShape                                       []uint32
		tmplMode                                        []uint8
		tmplOff                                         = []uint64{0}
		tmplBin                                         []byte
		tmplSegOff, tmplHoleOff                         = []uint32{0}, []uint32{0}
		tsegEnd, holeAt                                 []uint32
		holeW, holeEnc, holeKind                        []uint8
		holeRtype                                       []uint16
		tmplCount                                       []int
		code                                            []byte
		codeOff                                         = []uint64{0}
		oRelOff                                         []uint64
		oRelSiz                                         []uint8
		oRelType                                        []uint16
		oRelSym                                         []uint32
		oRelAdd                                         []int64
		nodeBytes, glueBytes, holesInGlue, relocsInGlue int64
		nNodes                                          int
		holeKinds                                       [8]int64
	)

	for fi, f := range fns {
		B := f.body
		base := uint64(len(code))
		code = append(code, B...)
		codeOff = append(codeOff, uint64(len(code)))
		for _, r := range f.relocs {
			oRelOff = append(oRelOff, base+uint64(r.off))
			oRelSiz = append(oRelSiz, r.siz)
			oRelType = append(oRelType, r.typ)
			oRelSym = append(oRelSym, syms.id(r.sym))
			oRelAdd = append(oRelAdd, r.add)
		}
		// the tree
		var top []int32
		for i := range f.nodes {
			n := &f.nodes[i]
			if n.start > n.end || n.end > int32(len(B)) {
				panic(fmt.Sprintf("%s: node %d [%d,%d) outside the body (%d)", f.name, i, n.start, n.end, len(B)))
			}
			if n.parent < 0 {
				top = append(top, int32(i))
			} else {
				p := &f.nodes[n.parent]
				if n.start < p.start || n.end > p.end {
					panic(fmt.Sprintf("%s: node %d not inside its parent", f.name, i))
				}
				p.kids = append(p.kids, int32(i))
			}
		}
		// items, in byte order
		var items []item
		nodeBase := uint32(len(nodeTmpl))
		var walk func(ni int32)
		walk = func(ni int32) {
			n := &f.nodes[ni]
			pos := n.start
			for s, k := range n.kids {
				c := &f.nodes[k]
				if c.start < pos {
					panic(fmt.Sprintf("%s: children of node %d overlap", f.name, ni))
				}
				items = append(items, item{1, nodeBase + uint32(ni), uint32(s), pos, c.start - pos})
				walk(k)
				pos = c.end
			}
			items = append(items, item{1, nodeBase + uint32(ni), uint32(len(n.kids)), pos, n.end - pos})
		}
		pos := int32(0)
		for _, k := range top {
			c := &f.nodes[k]
			if c.start < pos {
				panic(fmt.Sprintf("%s: top-level nodes overlap", f.name))
			}
			items = append(items, item{0, 0, 0, pos, c.start - pos})
			walk(k)
			pos = c.end
		}
		items = append(items, item{0, 0, 0, pos, int32(len(B)) - pos})
		itemBase := uint32(len(itemKind))
		// which item holds byte p (the last item starting at or before p that is not empty, or p == its end)
		starts := make([]int32, len(items))
		for i, it := range items {
			starts[i] = it.start
		}
		find := func(p int32) int {
			i := sort.Search(len(items), func(i int) bool { return starts[i] > p }) - 1
			j := i
			for j >= 0 && items[j].len == 0 && items[j].start == p {
				j--
			}
			if j >= 0 && items[j].start+items[j].len > p {
				return j
			}
			return i
		}
		// holes and relocations to their items
		holesOf := map[int][]hole{}
		for _, h := range f.holes {
			it := find(h.pc)
			if it < 0 || h.pc+int32(h.w) > items[it].start+items[it].len {
				panic(fmt.Sprintf("%s: hole at %d straddles items", f.name, h.pc))
			}
			holeKinds[h.kind]++
			if items[it].kind == 0 {
				holesInGlue++
				continue
			}
			switch h.kind {
			case kLabel:
				rel := int32(binary.LittleEndian.Uint32(B[h.pc:]))
				t := h.pc + 4 + rel
				if t < 0 || t > int32(len(B)) {
					panic(fmt.Sprintf("%s: jump at %d leaves the body", f.name, h.pc))
				}
				ti := find(t)
				h.val, h.aux = int64(itemBase)+int64(ti), int64(t-items[ti].start)
			default:
				var v int64
				switch h.w {
				case 1:
					v = int64(int8(B[h.pc]))
				case 2:
					v = int64(int16(binary.LittleEndian.Uint16(B[h.pc:])))
				case 4:
					v = int64(int32(binary.LittleEndian.Uint32(B[h.pc:])))
				case 8:
					v = int64(binary.LittleEndian.Uint64(B[h.pc:]))
				}
				h.val = v
			}
			holesOf[it] = append(holesOf[it], h)
		}
		mo := fnMarkOff[len(fnMarkOff)-1]
		glueRels := map[int][]reloc{}
		for _, r := range f.relocs {
			if r.siz == 0 {
				markOff = append(markOff, uint32(r.off))
				markType = append(markType, r.typ)
				markSym = append(markSym, syms.id(r.sym))
				markAdd = append(markAdd, r.add)
				mo++
				continue
			}
			it := find(r.off)
			if it < 0 || r.off+int32(r.siz) > items[it].start+items[it].len {
				panic(fmt.Sprintf("%s: relocation at %d straddles items", f.name, r.off))
			}
			if items[it].kind == 0 {
				relocsInGlue++
				glueRels[it] = append(glueRels[it], r)
				continue
			}
			holesOf[it] = append(holesOf[it], hole{pc: r.off, w: r.siz, kind: kSym, typ: r.typ, val: int64(syms.id(r.sym)), aux: r.add, isReloc: true})
		}
		fnMarkOff = append(fnMarkOff, mo)
		// glue items and their relocations
		gid := map[int]uint32{}
		for i, it := range items {
			if it.kind != 0 {
				continue
			}
			g := uint32(len(glueOff) - 1)
			gid[i] = g
			glueBin = append(glueBin, B[it.start:it.start+it.len]...)
			glueOff = append(glueOff, uint64(len(glueBin)))
			glueBytes += int64(it.len)
			for _, r := range glueRels[i] {
				grelOff = append(grelOff, uint32(r.off-it.start))
				grelSiz = append(grelSiz, r.siz)
				grelType = append(grelType, r.typ)
				grelSym = append(grelSym, syms.id(r.sym))
				grelAdd = append(grelAdd, r.add)
			}
			glueRelOff = append(glueRelOff, uint32(len(grelOff)))
		}
		// nodes: their templates (own bytes, holes zeroed) and operands
		segItems := make([][]int, len(f.nodes))
		for i, it := range items {
			if it.kind == 1 {
				ni := it.ref - nodeBase
				segItems[ni] = append(segItems[ni], i)
			}
		}
		for ni := range f.nodes {
			n := &f.nodes[ni]
			var tb []byte
			var segEnd []uint32
			var hs []hole
			var hat []uint32
			for _, ii := range segItems[ni] {
				it := items[ii]
				segBase := int32(len(tb))
				tb = append(tb, B[it.start:it.start+it.len]...)
				l := holesOf[ii]
				sort.Slice(l, func(a, b int) bool { return l[a].pc < l[b].pc })
				for k, h := range l {
					if k > 0 && l[k-1].pc+int32(l[k-1].w) > h.pc {
						panic(fmt.Sprintf("%s: holes overlap at %d", f.name, h.pc))
					}
					at := segBase + h.pc - it.start
					if !h.isReloc {
						for b := int32(0); b < int32(h.w); b++ {
							tb[at+b] = 0
						}
					}
					hs = append(hs, h)
					hat = append(hat, uint32(at))
				}
				segEnd = append(segEnd, uint32(len(tb)))
			}
			sid := shapes.id(n.shape)
			mid := modes.id(n.mode)
			var key strings.Builder
			fmt.Fprintf(&key, "%d|%d|%x|%v|", sid, mid, tb, segEnd)
			for k, h := range hs {
				fmt.Fprintf(&key, "%d,%d,%d,%d;", hat[k], h.w, h.kind, h.typ)
			}
			tid, ok := tmplIDs[key.String()]
			if !ok {
				tid = uint32(len(tmplShape))
				tmplIDs[key.String()] = tid
				tmplShape = append(tmplShape, sid)
				tmplMode = append(tmplMode, uint8(mid))
				tmplBin = append(tmplBin, tb...)
				tmplOff = append(tmplOff, uint64(len(tmplBin)))
				tsegEnd = append(tsegEnd, segEnd...)
				tmplSegOff = append(tmplSegOff, uint32(len(tsegEnd)))
				for k, h := range hs {
					holeAt = append(holeAt, hat[k])
					holeW = append(holeW, h.w)
					holeKind = append(holeKind, h.kind)
					holeRtype = append(holeRtype, h.typ)
					switch {
					case h.isReloc:
						holeEnc = append(holeEnc, encReloc)
					case h.kind == kLabel:
						holeEnc = append(holeEnc, encRel32)
					default:
						holeEnc = append(holeEnc, encInt)
					}
				}
				tmplHoleOff = append(tmplHoleOff, uint32(len(holeAt)))
				tmplCount = append(tmplCount, 0)
			}
			tmplCount[tid]++
			nodeTmpl = append(nodeTmpl, tid)
			nodeShape = append(nodeShape, sid)
			nodeMode = append(nodeMode, uint8(mid))
			par := uint32(math.MaxUint32)
			if n.parent >= 0 {
				par = nodeBase + uint32(n.parent)
			}
			nodeParent = append(nodeParent, par)
			nodeFn = append(nodeFn, uint32(fi))
			for _, h := range hs {
				opVal = append(opVal, h.val)
				opAux = append(opAux, h.aux)
			}
			nodeOp = append(nodeOp, uint32(len(opVal)))
			nNodes++
		}
		for i, it := range items {
			itemKind = append(itemKind, it.kind)
			if it.kind == 0 {
				itemRef = append(itemRef, gid[i])
			} else {
				itemRef = append(itemRef, it.ref)
				nodeBytes += int64(it.len)
			}
			itemSeg = append(itemSeg, it.seg)
		}
		fnItem = append(fnItem, uint32(len(itemKind)))
	}
	if len(modes.list) > 255 {
		panic("too many modes")
	}

	names := make([]string, len(fns))
	for i, f := range fns {
		names[i] = f.name
	}
	o.strings("fn_name", names)
	o.write("fn_item.u32", fnItem)
	o.write("fn_mark_off.u32", fnMarkOff)
	o.write("mark_off.u32", markOff)
	o.write("mark_type.u16", markType)
	o.write("mark_sym.u32", markSym)
	o.write("mark_add.i64", markAdd)
	o.write("item_kind.u8", itemKind)
	o.write("item_ref.u32", itemRef)
	o.write("item_seg.u32", itemSeg)
	o.write("glue.off.u64", glueOff)
	o.write("glue.bin", glueBin)
	o.write("glue_rel_off.u32", glueRelOff)
	o.write("grel_off.u32", grelOff)
	o.write("grel_siz.u8", grelSiz)
	o.write("grel_type.u16", grelType)
	o.write("grel_sym.u32", grelSym)
	o.write("grel_add.i64", grelAdd)
	o.write("node_tmpl.u32", nodeTmpl)
	o.write("node_shape.u32", nodeShape)
	o.write("node_mode.u8", nodeMode)
	o.write("node_parent.u32", nodeParent)
	o.write("node_fn.u32", nodeFn)
	o.write("node_op.u32", nodeOp)
	o.write("op_val.i64", opVal)
	o.write("op_aux.i64", opAux)
	o.write("tmpl_shape.u32", tmplShape)
	o.write("tmpl_mode.u8", tmplMode)
	o.write("tmpl.off.u64", tmplOff)
	o.write("tmpl.bin", tmplBin)
	o.write("tmpl_seg_off.u32", tmplSegOff)
	o.write("tseg_end.u32", tsegEnd)
	o.write("tmpl_hole_off.u32", tmplHoleOff)
	o.write("hole_at.u32", holeAt)
	o.write("hole_w.u8", holeW)
	o.write("hole_enc.u8", holeEnc)
	o.write("hole_kind.u8", holeKind)
	o.write("hole_rtype.u16", holeRtype)
	o.strings("shape", shapes.list)
	o.strings("mode", modes.list)
	o.strings("sym", syms.list)
	o.write("oracle/p4_code.bin", code)
	o.write("oracle/p4_fn_off.u64", codeOff)
	o.write("oracle/p4_rel_off.u64", oRelOff)
	o.write("oracle/p4_rel_siz.u8", oRelSiz)
	o.write("oracle/p4_rel_type.u16", oRelType)
	o.write("oracle/p4_rel_sym.u32", oRelSym)
	o.write("oracle/p4_rel_add.i64", oRelAdd)

	meta, err := os.Create(filepath.Join(o.dir, "meta.txt"))
	check(err)
	fmt.Fprintf(meta, "nfn %d\nnitem %d\nnglue %d\nnnode %d\nntmpl %d\nnshape %d\nnmode %d\nnsym %d\nnop %d\nnthole %d\nntseg %d\nnmark %d\nngrel %d\ncode_bytes %d\nnrel %d\n",
		len(fns), len(itemKind), len(glueOff)-1, len(nodeTmpl), len(tmplShape), len(shapes.list), len(modes.list), len(syms.list),
		len(opVal), len(holeAt), len(tsegEnd), len(markOff), len(grelOff), len(code), len(oRelOff))
	check(meta.Close())

	// ---- stats
	fmt.Printf("functions %d, body bytes %d: in expression nodes %d (%.1f%%), glue %d\n", len(fns), len(code), nodeBytes, 100*float64(nodeBytes)/float64(len(code)), glueBytes)
	fmt.Printf("holes recorded by kind (frame out disp imm framesize label): %v; in glue %d; relocations in glue %d of %d\n", holeKinds[1:7], holesInGlue, relocsInGlue, len(oRelOff))
	fmt.Printf("nodes %d, typed shapes %d, modes %d, templates %d, operands %d\n", nNodes, len(shapes.list)-1, len(modes.list)-1, len(tmplShape), len(opVal))
	curve := func(what string, counts []int, total int) {
		sort.Sort(sort.Reverse(sort.IntSlice(counts)))
		fmt.Printf("%s: %d distinct;", what, len(counts))
		acc := 0
		next := 256
		for i, c := range counts {
			acc += c
			if i+1 == next {
				fmt.Printf(" top %d %.1f%%", next, 100*float64(acc)/float64(total))
				next *= 4
			}
		}
		fmt.Println()
	}
	sc := map[uint32]int{}
	sm := map[[2]uint32]int{}
	for i := range nodeShape {
		sc[nodeShape[i]]++
		sm[[2]uint32{nodeShape[i], uint32(nodeMode[i])}]++
	}
	vals := func(m map[uint32]int) (l []int) {
		for _, v := range m {
			l = append(l, v)
		}
		return
	}
	vals2 := func(m map[[2]uint32]int) (l []int) {
		for _, v := range m {
			l = append(l, v)
		}
		return
	}
	curve("typed shapes (wholeprog's key)", vals(sc), nNodes)
	curve("typed shape + mode", vals2(sm), nNodes)
	curve("byte templates", append([]int(nil), tmplCount...), nNodes)
	// how much of a node's template its (shape, mode) decides: nodes in the commonest template of their key
	best := map[[2]uint32]int{}
	for t, c := range tmplCount {
		k := [2]uint32{tmplShape[t], uint32(tmplMode[t])}
		if c > best[k] {
			best[k] = c
		}
	}
	nb := 0
	for _, c := range best {
		nb += c
	}
	fmt.Printf("nodes whose template is the commonest of their (shape, mode): %.1f%%\n", 100*float64(nb)/float64(nNodes))
}

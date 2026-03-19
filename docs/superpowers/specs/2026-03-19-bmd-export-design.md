# BMD Export for J3DBlend

## Overview

Add BMD (Binary Model) export to J3DBlend, the Blender 5.0 addon for Nintendo GameCube J3D formats. Enables full round-trip: import a BMD, edit geometry/materials/skeleton in Blender, export back to a byte-compatible BMD.

**Endianness:** BMD is big-endian (GameCube). BinaryWriter handles byte swapping on little-endian host machines.

## Scope

- v1: Full round-trip export with preserved TEV data + basic material presets for new materials
- v2 (future): Visual TEV combiner editor panel in Blender

## Architecture

The import code is the spec. Each import module (Inf1.py, Vtx1.py, etc.) reads binary into Python objects. The export adds DumpData/SaveData methods to each module that writes those same objects back to binary.

**Data flow:**
```
Import: BMD binary -> BinaryReader -> Python objects -> Blender scene
Export: Blender scene -> Python objects -> BinaryWriter -> BMD binary
```

**Round-trip strategy:** During import, store raw section data as custom properties on Blender objects. On export, check if data was modified - if not, write back the stored original. This guarantees byte-identical output for untouched sections.

**For modified data:** Reconstruct Python objects from Blender scene data, apply coordinate conversions (same inverse approach as BCK export), then serialize.

Export methods are added to existing module files rather than creating new ones. Each Foo.py gets a DumpData method alongside its LoadData.

**Existing export code inventory:**
- BModel_out.py (381 lines): Armature parsing, weight extraction, batch splitting - PARTIAL
- Inf1.py: Has DumpData + extractEntries - BUGGY (extractEntries never appends hierarchy entries)
- Jnt1.py: Has DumpData - NEEDS VERIFICATION
- Evp1.py: Has DumpData - NEEDS VERIFICATION
- Shp1.py: Has DumpPacketPrimitives - BROKEN (clears points list before iterating, writes zero geometry)
- Bck.py: Has full working DumpData export - REFERENCE IMPLEMENTATION

All existing DumpData methods need audit and likely rewrite before use.

## File Format

```
File Header (32 bytes):
  "J3D2bmd3"          - 8 bytes magic
  u32 file_size       - backfilled after all sections written
  u32 section_count   - 8 for standard BMD (9 if MDL3 present)
  16 bytes padding    - zeros or SVR3 tag

Sections (written in this order):
  1. INF1 - Scene graph
  2. VTX1 - Vertex data
  3. EVP1 - Skinning weights
  4. DRW1 - Draw matrices
  5. JNT1 - Joints
  6. SHP1 - Shapes/geometry
  7. MAT3 - Materials
  8. TEX1 - Textures

Each section:
  4-byte magic tag
  u32 section_size (includes header)
  section data
  pad to 32-byte alignment at section boundaries
  (internal sub-tables may use 16-byte alignment - verify per section against import code)

MDL3 (display list cache) may be present in some BMD files (section_count=9).
v1 does not generate MDL3. Files imported with MDL3 will have it stored and
written back as a raw binary blob.
```

## Build Order (dependency resolution)

1. Import-side custom property storage (needed before any export testing)
2. JNT1 (skeleton - no deps, we understand joints best from BCK work)
3. VTX1 (vertices - no deps)
4. TEX1 (textures - no deps)
5. EVP1 (needs JNT1 indices)
6. DRW1 (needs JNT1 + EVP1 indices)
7. MAT3 (needs TEX1 indices)
8. SHP1 (needs VTX1 + DRW1 indices)
9. INF1 (needs all indices)
10. BMD file orchestrator (header, section assembly, size backfill)
11. Material preset detection
12. Round-trip testing
13. In-game testing via GCFT + Dolphin

## Per-Section Export Strategy

### JNT1 (Joints) - Easy

- Read Blender armature bones
- Use stored gc_rest custom properties (rx/ry/rz/tx/ty/tz) for unmodified bones
- For modified bones: decompose Blender bone matrix to GC space via BtoN conversion
- Rotation encoding: s16 = round(radians * 32768 / pi)
- Write: name string table, index remap table, 0x40-byte joint entries
- Entry layout (0x40 = 64 bytes):
  - u16 matrix_type
  - u16 padding
  - f32 scaleX, f32 scaleY, f32 scaleZ
  - s16 rotationX, s16 rotationY, s16 rotationZ
  - u16 padding2
  - f32 translateX, f32 translateY, f32 translateZ
  - f32 unknown2
  - f32 bbMinX, f32 bbMinY, f32 bbMinZ
  - f32 bbMaxX, f32 bbMaxY, f32 bbMaxZ

### VTX1 (Vertices) - Medium

- Extract from Blender mesh: positions, normals, UVs, vertex colors
- Use f32 for positions/normals/UVs (simplest encoding, no fixed-point scaling)
- Deduplicate vertex attributes into pools (position pool, normal pool, UV pool, etc.)
- Write: array format descriptors (16 bytes each, NOT 12) + raw data arrays
- Format descriptor (16 bytes):
  - u32 arrayType (0x9=position, 0xa=normal, 0xb/0xc=color, 0xd-0x14=texcoord)
  - u32 componentCount
  - u32 dataType (3=s16, 4=f32, 5=rgba8)
  - u8 decimalPoint (fixed-point scale, 0 for f32)
  - u8 unknown3 (always 0xFF)
  - u16 unknown4 (always 0xFFFF)
- 13 possible array slots: 1 position, 1 normal, 2 color, 8 texcoord, plus NBT

### TEX1 (Textures) - Medium

- For unmodified textures: write back stored original binary data
- For new/modified textures: encode Blender image to RGBA8 format (simplest GC texture format)
- CMPR (DXT1) compression as optional future optimization
- Texture header (32 bytes, all fields must be preserved for round-trip):
  - u8 format
  - u8 unknown1
  - u16 width
  - u16 height
  - u8 wrapS (0=clamp, 1=repeat, 2=mirror)
  - u8 wrapT
  - u8 unknown3 (preserve on round-trip)
  - u8 paletteFormat
  - u16 paletteNumEntries
  - u32 paletteOffset
  - u32 unknown5 (preserve)
  - u16 unknown6 (preserve)
  - u16 unknown7 (preserve)
  - u8 mipmapCount
  - u8 unknown8 (preserve)
  - u16 unknown9 (preserve)
  - u32 dataOffset
- Write: texture headers + string table + image data

### EVP1 (Skinning Weights) - Medium

- Extract vertex group weights from Blender mesh
- Deduplicate weight patterns (BModel_out.py already implements this)
- Compute inverse bind matrices as 3x4 matrices (12 floats, bottom row [0,0,0,1] is implicit)
- Formula: (parent_world_matrix @ bone_local_matrix).inverted(), then take top 3 rows
- Write: u8 weight counts, s16 bone indices, f32 weights, f32x12 inverse bind matrices
- Concatenated flat arrays with counts indexing into them

### DRW1 (Draw Matrices) - Easy

- For each vertex batch: single-bone = rigid (isWeighted=0), multi-bone = smooth (isWeighted=1)
- Write: u8 isWeighted array + u16 indices array (joint index for rigid, EVP1 index for smooth)

### SHP1 (Shapes/Geometry) - Medium-Hard

**NOTE: Existing DumpPacketPrimitives in Shp1.py is broken (clears points list at line 136 before iterating). Must be rewritten from scratch.**

- Group Blender faces by material into batches
- Split batches by bone count (max 10 per packet - GC hardware limit)
- v1: emit raw triangles (primitive type 0x90), skip triangle strip optimization
- Triangle strip optimization as future improvement
- Write: batch descriptors (40 bytes), attribute lists, primitive data blocks, matrix tables, packet locations
- Batch attributes define which vertex data is present: position(0x9), normal(0xa), color(0xb/0xc), texcoord(0xd-0x14), matrix(0x0)
- Vertex indices: per-attribute index (posIndex, normalIndex, colorIndex, texCoordIndex) packed per primitive vertex
- Index size: s8 (1 byte) if pool < 256 entries, s16 (2 bytes) otherwise
- Matrix index encoding: index * 3, with offset 30 (matches GC XF matrix memory layout)

### MAT3 (Materials) - Hard

- For unmodified materials: write back stored original TEV/material data from custom properties
- For new materials: detect pattern and apply preset (see Material Presets section)
- 30 subsections with heavy deduplication
- Each material stores short indices into shared pools (color pool, TEV stage pool, blend pool, etc.)
- Index -1 means unused slot
- Write: material entries, string table, then all 30 sub-tables with proper offset tracking

### INF1 (Scene Graph) - Easy

**NOTE: Existing extractEntries in Inf1.py is buggy (never appends hierarchy up/down entries). Must be fixed.**

- Walk Blender object hierarchy
- Emit flat node list with depth markers:
  - 0x10 = Joint node (u16 index into JNT1)
  - 0x11 = Material node (u16 index into MAT3)
  - 0x12 = Shape node (u16 index into SHP1)
  - 0x01 = Open child (push hierarchy)
  - 0x02 = Close child (pop hierarchy)
  - 0x00 = Terminator
- Header includes: u16 transform_mode, u16 padding, u32 packet_count, u32 vertex_count, u32 hierarchy_offset (always 0x18)
- Pattern per mesh: Joint -> Open -> Material -> Open -> Shape -> Close -> Close

## Round-Trip Preservation

Custom properties stored during import to enable byte-identical export of unmodified data:

| Data | Stored On | Property Name | Purpose |
|------|-----------|--------------|---------|
| Joint rest pose | Bone | gc_rest_rx/ry/rz/tx/ty/tz | Already implemented for BCK |
| Joint scale | Bone | gc_rest_sx/sy/sz | Preserve original scale |
| Joint bounding box | Bone | gc_bb_min_x/y/z, gc_bb_max_x/y/z | Preserve bounds |
| Joint matrix type | Bone | gc_matrix_type | Preserve u16 field |
| Material TEV config | Material | gc_mat_data (bytes) | Full MAT3 entry as binary blob |
| Texture header + data | Image | gc_tex_header (bytes), gc_tex_data (bytes) | Full original encoding |
| Original vertex format | Mesh | gc_vtx_format (bytes) | Preserve fixed-point vs float |
| MDL3 raw data | Object | gc_mdl3_data (bytes) | Write back if present |

**Modified detection:** Compare Blender state against stored properties. Match = write original. Mismatch = reconstruct from Blender.

**Always reconstructed from Blender (never stored):**
- Vertex positions, normals, UVs (mesh geometry is live data)
- Face topology
- Bone hierarchy
- Vertex weights

## Material Presets (New Materials)

When a material has no stored TEV data (created in Blender, not imported), auto-detect the node tree pattern and apply a preset. Preset TEV configurations should be validated against SuperBMD's default material generation for game compatibility.

**Preset 1: Textured Opaque**
- Detect: Principled BSDF with image texture on Base Color
- TEV: 1 stage, sample texture, output to framebuffer
- Alpha disabled, cull backfaces, depth test on

**Preset 2: Textured + Vertex Color**
- Detect: Image texture multiplied with vertex color
- TEV: 1 stage, texture * rasterized vertex color

**Preset 3: Textured Transparent**
- Detect: Image texture with alpha connected to Alpha input
- TEV: 1 stage, texture with alpha
- Alpha compare from Blender alpha clip threshold
- Blend: src_alpha / one_minus_src_alpha

**Preset 4: Solid Color**
- Detect: No image texture, just Base Color value
- TEV: 1 stage, constant color passthrough

**Preset 5: Vertex Color Only**
- Detect: Vertex color node connected directly
- TEV: 1 stage, rasterized color passthrough

**Fallback:** If no pattern matches, use Preset 1 with a white 4x4 placeholder texture.

## Coordinate Conversions

All conversions verified against working BCK export and SuperBMD reference.

**Positions and Translations:**
- Import (BModel.py toarray): gc(x,y,z) -> blender(x, -z, y)
- Export (BModel_out.py): blender(x,y,z) -> gc(x, z, -y)
- Equivalent: gc_x = blender_x, gc_y = blender_z, gc_z = -blender_y

**Rotations:**
- Use stored gc_rest custom properties for unmodified bones (exact JNT1 values)
- For modified bones: decompose Blender matrix via BtoN sandwich
- Euler order: XYZ intrinsic (ZYX extrinsic) - confirmed from J3DGetTranslateRotateMtx in decomp

**Normals:**
- Same conversion as positions: gc_x = blender_x, gc_y = blender_z, gc_z = -blender_y

**UVs:**
- V flip: gc_v = 1.0 - blender_v
- U unchanged

**Scale:**
- Y/Z swap, no negation: gc_x = blender_x, gc_y = blender_z, gc_z = blender_y

## Testing

### Test 1: Structurally valid round-trip (no edits)
- Import ma_mdl1.bmd -> export immediately
- Import the exported BMD in blemd -> verify it loads without errors
- Load in SMS via GCFT repack -> verify in Dolphin (model displays correctly)
- STRETCH GOAL: binary diff against original for byte-identical output

### Test 2: Geometry edit round-trip
- Import BMD -> move vertices -> export
- Import exported BMD -> verify mesh matches edits
- Load in SMS via GCFT repack -> verify in Dolphin

### Test 3: Skeleton edit round-trip
- Import BMD -> reposition a bone -> export
- Verify joint data changed, rest of file intact
- Test in-game

### Test 4: New material
- Import BMD -> assign new Blender material with texture
- Export -> verify TEV preset applied
- Test in-game

### Test 5: Simple new mesh
- Create cube in Blender with texture and simple armature
- Export as BMD from scratch
- Import exported BMD to verify round-trip
- Load in SMS as custom object

### Test 6: Cape model
- Model cape mesh in Blender
- Rig to Mario's skeleton or new bone chain
- Export as BMD
- Load in SMS via BSE module

## References

- blemd import code: each section's LoadData is the format spec
- SuperBMD (C#): https://github.com/RenolY2/SuperBMD - known-working BMD exporter
- SMS decomp: C:\Users\ryana\documents\sms\ - J3D headers in include/JSystem/J3D/
- delfino UE plugin: https://github.com/ryanbevins/delfino - BCK/BMD loader reference
- BModel_out.py: 381 lines of partial export (armature parsing, weight extraction, batch splitting)
- Existing DumpData methods in Inf1.py, Jnt1.py, Evp1.py, Shp1.py (all need audit/rewrite)

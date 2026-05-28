# Plan: Single-File NVDLA Loadable Parser in `parse.py`

## Goal

Implement a self-contained Python parser for NVDLA loadable files, matching the behavior of `nvdla-parser/` while keeping all implementation in `parse.py`.

The first target is a practical inspection/parser tool, not a full compiler or executor. It should load a `.nvdla` FlatBuffer, dump all top-level loadable lists, resolve task address entries to memory entries and blobs, and decode the embedded DLA firmware descriptor blobs described by `dla_interface.h`.

## References

- `nvdla-parser/external/loadable_generated.h`: FlatBuffer schema as generated C++ accessors.
- `nvdla-parser/include/dla_interface.h`: Packed C structs for raw DLA descriptor blobs.
- `nvdla-parser/src/Interface.cpp`: File loading and FlatBuffer verification flow.
- `nvdla-parser/src/Parser.cpp`: Core resolution flow from task address list to blob data.
- `nvdla-parser/examples/*.cpp`: Expected list dumping behavior.
- `nvdla-parser/data/lenet-mnist-caffe/fast-math.nvdla`: Likely validation fixture.

## Constraints

- Keep implementation in one file: `parse.py`.
- Prefer Python standard library for raw struct decoding.
- Use the `flatbuffers` Python package if available. If not available, document install requirement and fail with a clear message.
- Do not generate separate Python schema files unless a later requirement explicitly allows it.
- Treat all multi-byte fields as little-endian.
- Match C packed/aligned layout from `dla_interface.h`; verify with `struct.calcsize()` against known C layout assumptions where possible.

## Milestones

### 1. Command-Line Skeleton

- Add `argparse` CLI in `parse.py`.
- Required positional argument: loadable path.
- Add output mode flags:
  - `--summary`: print version and counts for top-level lists.
  - `--lists`: dump task, memory, address, event, blob, tensor, reloc, submit lists.
  - `--descs`: decode network/common/surface/operation descriptor blobs.
  - `--json`: emit JSON instead of human-readable text.
  - `--dump-blob NAME_OR_INDEX`: print blob metadata and optional hex preview.
  - `--hex-bytes N`: default hex preview length.
- Default behavior should be useful: run `--summary --descs` if no mode flag is selected.
- Return non-zero exit codes for missing file, invalid FlatBuffer, unresolved references, or decode errors.

### 2. Minimal FlatBuffer Reader

Implement a small FlatBuffer accessor layer directly in `parse.py` so generated Python files are not required.

- Implement helpers:
  - `read_u8`, `read_i8`, `read_u16`, `read_i16`, `read_u32`, `read_i32`, `read_u64`, `read_i64`.
  - `read_root_table(buf)`: root table offset at byte 0.
  - `table_field_offset(buf, table_pos, vtable_offset)`: return absolute field position or `None`.
  - `table_scalar(buf, table_pos, vtable_offset, fmt, default)`.
  - `table_struct_pos(buf, table_pos, vtable_offset)` for inline structs like `Version`.
  - `table_string(buf, field_pos)` for FlatBuffer string offsets.
  - `table_vector_pos(buf, field_pos)` for vector offsets.
  - `vector_len(buf, vec_pos)`.
  - `vector_scalar(buf, vec_pos, index, fmt)`.
  - `vector_table(buf, vec_pos, index)`.
  - `vector_bytes(buf, vec_pos)` returning `bytes` or `memoryview`.
- Add bounds checks to every helper so corrupt files fail clearly.
- Validate root table shape enough for practical use:
  - Root offset is in range.
  - Root vtable is in range.
  - Required `version` field exists.
  - Vectors, when present, point inside the file.

### 3. Encode Loadable Schema Constants

Add constants from `loadable_generated.h`.

- Interfaces:
  - `NONE = 0`, `DLA1 = 1`, `EMU1 = 2`.
- Memory domains:
  - `SYSTEM = 0`, `SRAM = 1`.
- Memory flags:
  - `ALLOC = 1`, `SET = 2`, `INPUT = 4`, `OUTPUT = 8`.
- Blob sub-interface labels used by examples:
  - `0:NONE`, `1:ADDR0`, `2:DEPS`, `3:OPS`, `4:SURFS`, `5:LUTS`.
- Top-level `Loadable` vtable offsets:
  - `version=4`, `task_list=6`, `memory_list=8`, `address_list=10`, `event_list=12`, `blobs=14`, `tensor_desc_list=16`, `reloc_list=18`, `submit_list=20`.
- Table field vtable offsets for:
  - `Blob`
  - `MemoryListEntry`
  - `EventListEntry`
  - `TaskListEntry`
  - `AddressListEntry`
  - `SubmitListEntry`
  - `TensorDescListEntry`
  - `RelocListEntry`

### 4. Model Top-Level Loadable Entries

Use small `@dataclass` models or dictionaries for parsed output.

- `Version`: `major`, `minor`, `sub_minor` from inline 3-byte struct.
- `Blob`: `name`, `size`, `interface`, `sub_interface`, `version`, `data`.
- `MemoryListEntry`: `id`, `domain`, `flags`, `size`, `alignment`, `contents`, `offsets`, `bind_id`, `tensor_desc_id`.
- `AddressListEntry`: `id`, `mem_id`, `offset`, `size`.
- `TaskListEntry`: `id`, `interface`, `instance`, `address_list`, `pre_actions`, `post_actions`.
- `EventListEntry`: `id`, `type`, `target`, `val`, `op`.
- `TensorDescListEntry`: all fields printed by example 7, including `mem_id` from generated header.
- `RelocListEntry`: `address_id`, `write_id`, `offset`, `interface`, `sub_interface`, `reloc_type`.
- `SubmitListEntry`: `id`, `task_id` vector.
- `Loadable`: owns all parsed lists plus indexes for fast lookup.

### 5. Build Indexes and Resolution Helpers

Mirror `Parser.cpp` logic with safer error handling.

- Build `address_by_id`: address entry id to entry.
- Build `memory_by_id`: memory entry id to entry.
- Build `blob_by_name`: blob name to blob index/object.
- Build `addr_to_mem_id`: address id to memory id.
- Build `mem_id_to_alias`: memory id to first `contents` string or empty string.
- Implement helpers:
  - `get_memory_for_address(address_id)`.
  - `get_name(address_id)`: address id to memory content alias.
  - `get_blob_index(name)`.
  - `find_address_blob_index(address_id)`.
  - `find_blob_index_from_task(task_idx, address_slot)`: use task's address-list slot, then resolve to blob.
- Important correction over C++ examples: do not assume `address_list->Get(address_idx)` is the same as entry id unless validated. Prefer ID-based maps, but include a compatibility fallback if fixture files encode dense ids.

### 6. Decode DLA Firmware Descriptor Structs

Implement raw binary decoders based on `dla_interface.h` using `struct.unpack_from`.

Start with the descriptors needed by `Parser.cpp`:

- `dla_network_desc`
  - Format: 8 signed 16-bit fields, 6 signed 16-bit `op_head`, 4 unsigned 16-bit fields, signed 16-bit `input_layer`, unsigned 8-bit `dynamic_roi`, unsigned 8-bit `reserved0`.
  - Decode fields: `operation_desc_index`, `surface_desc_index`, `dependency_graph_index`, `lut_data_index`, `roi_array_index`, `surface_index`, `stat_list_index`, `reserved1`, `op_head`, `num_rois`, `num_operations`, `num_luts`, `num_addresses`, `input_layer`, `dynamic_roi`.
- `dla_common_op_desc`
  - Decode array entries according to `num_operations` from network descriptor.
  - Include `index`, `roi_index`, `op_type`, `dependency_count`, `consumers[6]`, `fused_parent`.
- `dla_data_cube`
  - Shared by surface descriptors.
  - Include `type`, `address`, `offset`, `size`, `width`, `height`, `channel`, `line_stride`, `surf_stride`, `plane_stride`.
- Surface descriptors:
  - `dla_conv_surface_desc`.
  - `dla_sdp_surface_desc`.
  - `dla_pdp_surface_desc`.
  - `dla_cdp_surface_desc`.
  - `dla_rubik_surface_desc`.
  - `dla_bdma_surface_desc` can be added after core conv/sdp/pdp path is working.
- Operation descriptors:
  - `dla_conv_op_desc` first, because pure NPU register driver work likely needs conv register programming.
  - Add `dla_sdp_op_desc`, `dla_pdp_op_desc`, `dla_cdp_op_desc`, `dla_rubik_op_desc`, `dla_bdma_op_desc` after surface decoding is validated.

### 7. Descriptor Array Parsing Flow

Recreate the C++ `Parser` constructor flow in Python.

- Iterate every task in `task_list`.
- Only process tasks with `interface == DLA1`.
- Resolve task address slot 0 to network descriptor blob.
- Decode `network_desc`.
- Use network descriptor address indexes to resolve additional blobs through the task address list:
  - `dependency_graph_index` -> common op descriptor blob.
  - `surface_desc_index` -> surface descriptor blob.
  - `operation_desc_index` -> operation descriptor blob.
  - `lut_data_index`, `roi_array_index`, `surface_index`, `stat_list_index` can initially be reported as unresolved/unsupported unless needed.
- Decode arrays using `network_desc.num_operations`:
  - Common op array: `num_operations` entries of `dla_common_op_desc`.
  - Surface container array: one union-sized container per operation. Select member using corresponding common op `op_type`.
  - Operation container array: one union-sized container per operation. Select member using corresponding common op `op_type`.
- Define op type labels:
  - `0:BDMA`, `1:CONV`, `2:SDP`, `3:PDP`, `4:CDP`, `5:RUBIK`.
- Output each layer as:
  - operation index
  - op type
  - common descriptor
  - surface descriptor decoded as the matching op type
  - operation descriptor decoded as the matching op type
  - resolved source/destination/weight address aliases where available.

### 8. Binary Layout Validation

Add local checks in `parse.py` before decoding descriptor blobs.

- Maintain constants for expected sizes:
  - `DLA_NETWORK_DESC_SIZE`
  - `DLA_CONSUMER_SIZE`
  - `DLA_COMMON_OP_DESC_SIZE`
  - `DLA_DATA_CUBE_SIZE`
  - per-surface struct sizes
  - per-operation struct sizes
  - union container sizes as max member size.
- Check each blob has enough bytes before every decode.
- Print clear errors like `blob surfs too small for 10 surface containers: need X, have Y`.
- Keep all layout format strings near the struct decoder they support.
- Add a `--self-check-layouts` mode that prints calculated sizes for manual comparison with C if needed.

### 9. Human-Readable Reporting

Implement readable output similar to examples, but more compact.

- Summary output:
  - file path
  - loadable version
  - counts for every top-level list
  - blob names and sizes
- List output:
  - one section per list
  - enum fields displayed as `value:name`.
  - memory flags displayed as bit names.
- Descriptor output:
  - network descriptor first
  - then operation table sorted by common descriptor index or array order
  - for data cubes, include resolved address name if possible.
- JSON output:
  - must serialize bytes as metadata only by default (`size`, optional `sha256`, optional `hex_preview`).
  - no raw full blob data unless a future explicit flag is added.

### 10. Validation Against Existing C++ Parser

Use the fixture in `nvdla-parser/data` if present.

- Run C++ examples or inspect their expected printed fields where building is easy.
- Run Python parser:
  - `python3 parse.py nvdla-parser/data/lenet-mnist-caffe/fast-math.nvdla --summary`
  - `python3 parse.py nvdla-parser/data/lenet-mnist-caffe/fast-math.nvdla --lists`
  - `python3 parse.py nvdla-parser/data/lenet-mnist-caffe/fast-math.nvdla --descs`
  - `python3 parse.py nvdla-parser/data/lenet-mnist-caffe/fast-math.nvdla --json`
- Compare these fields against C++ examples:
  - version
  - list counts
  - task ids/interfaces/address lists
  - memory ids/contents/offsets
  - address ids/mem ids/sizes
  - blob names/sizes/interfaces/sub-interfaces
  - tensor descriptor dimensions and strides
  - network descriptor indexes and `num_operations`.

### 11. Error Handling and Diagnostics

- Raise a custom `ParseError` for corrupt or unsupported input.
- Include context in all errors: list name, entry index, field name, byte offset when available.
- Detect duplicate ids in memory/address lists and warn or fail depending on severity.
- Detect memory contents names that do not map to any blob.
- Detect task address slots referenced by network descriptor that are out of range.
- Detect unknown `op_type`; still print raw container hex preview instead of crashing.

### 12. Integration With Pure NPU Register Driver Work

After descriptor decoding is stable, add helpers that expose information needed by register programming.

- For CONV ops, report:
  - source/destination/weight cube dimensions and strides.
  - weight data, WMB, WGS addresses.
  - precision, data format, weight format, stride, padding, dilation.
  - CBUF bank fields: `data_bank`, `weight_bank`.
  - reuse/release fields: `data_reuse`, `weight_reuse`, `skip_data_rls`, `skip_weight_rls`, `release`.
- For SDP/PDP/CDP/RUBIK ops, report fields that drive register setup.
- Add optional `--ops conv,sdp,...` filter for focused dumps.
- Add optional `--emit-driver-json` later if the driver needs a stable machine-readable handoff.

## Implementation Order

1. Implement FlatBuffer helper layer and loadable root parsing.
2. Parse version and all top-level lists into dataclasses.
3. Add summary and list text output.
4. Add indexes and address/memory/blob resolution helpers.
5. Decode `dla_network_desc` and resolve descriptor blobs from DLA1 tasks.
6. Decode `dla_common_op_desc` array.
7. Decode `dla_data_cube`, CONV/SDP/PDP surface descriptors, and CONV operation descriptor.
8. Decode remaining operation/surface descriptor types.
9. Add JSON output.
10. Validate against `nvdla-parser` examples and fixture loadable.
11. Add driver-oriented reporting fields for CONV first.

## Acceptance Criteria

- `parse.py` can open a valid `.nvdla` file and print loadable version/counts.
- It dumps all top-level lists with values matching `nvdla-parser/examples`.
- It resolves memory contents names to blobs through task address lists.
- It decodes network descriptor fields and uses them to locate common/surface/operation descriptor blobs.
- It decodes at least CONV, SDP, and PDP descriptors without crashing on the Lenet fixture.
- It emits useful errors for malformed files and unresolved references.
- The file remains standalone: no project-local generated modules are required.

## Open Questions

- Should the parser require the `flatbuffers` Python package for verification, or should it remain pure standard library with only structural bounds checks?
- Should address resolution use strict ID-based lookup only, or keep compatibility with dense-index behavior from the C++ examples?
- Do we need byte-exact descriptor dumps for every op type now, or is CONV-first enough for the pure NPU register driver milestone?

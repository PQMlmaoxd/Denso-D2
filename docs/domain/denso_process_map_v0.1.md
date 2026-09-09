# DENSO process map v0.1 — biểu mẫu debrief

**TRẠNG THÁI: CHƯA ĐIỀN / CHƯA CÓ QUAN SÁT DENSO.**
Ngày khảo sát: … | Người ghi: … | Người kiểm tra: …

## 1. Ranh giới mô hình

- Pain point được quan sát: …
- Điểm đầu / điểm cuối của lát cắt: …
- Entity được mô phỏng và đơn vị: …
- Ca / khoảng thời gian quan sát: …
- Phần nằm ngoài mô hình: …
- Vì sao lát cắt này đủ để thử một quyết định hữu ích: …

## 2. Sơ đồ và topology

Chèn sơ đồ dựa trên Phiếu A; ghi mũi tên và ID tương ứng. Không sử dụng sơ đồ
toy Warehouse–Transport–Buffer–A–B như một sơ đồ DENSO đã xác nhận.

| Node ID | Ý nghĩa | Input / output | Nguồn chứng cứ | Unknown |
|---|---|---|---|---|
| … | … | … | … | … |

| Route ID | From → To | Resource | Batch/load | Tham số thời gian | Nguồn |
|---|---|---|---|---|---|
| … | … | … | … | … | … |

## 3. Resource, queue, buffer

| ID | Capacity và đơn vị | Shared với ai | Khi chiếm/nhả | Khi đầy/rỗng | Quy tắc phục vụ | Nguồn |
|---|---|---|---|---|---|---|
| … | … | … | … | … | … | … |

## 4. Entity và event

| Entity/event | Trigger | State trước → sau | Tham số | Timestamp/nguồn | Giả định còn cần |
|---|---|---|---|---|---|
| … | … | … | … | … | … |

## 5. Parameter registry

| ID | Gắn với node/route | Value/range | Unit | observed/estimated/assumed | Nguồn, n, thời điểm | Confidence | Cần mentor? |
|---|---|---|---|---|---|---|---|
| … | … | … | … | … | … | … | … |

Đây là bảng ghi nhận, chưa phải schema thực thi. Khánh và Hưng quyết định
mapping sang config; dữ liệu nhạy cảm chỉ đưa vào bản được phép chia sẻ.

## 6. Giả thuyết, baseline, action và KPI

| Hypothesis | Bằng chứng | Giải thích thay thế | Baseline | What-if | KPI để kiểm tra | Cần ai xác nhận? |
|---|---|---|---|---|---|---|
| … | … | … | … | … | … | … |

## 7. Unknown và Go/No-Go ngày 14/09

| Unknown | Nếu sai thì mô hình sai thế nào? | Ai phụ trách làm rõ? | Hạn | Kết quả |
|---|---|---|---|---|
| … | … | … | … | … |

- [ ] Một flow đủ rõ.
- [ ] Ít nhất một pain point có căn cứ.
- [ ] 2–3 action thực tế và giới hạn được mô tả.
- [ ] KPI có thể đo và có định nghĩa.
- [ ] Đủ tham số để dựng mô phỏng ước tính có provenance.
- [ ] Hưng hiểu flow; Khánh map được entity/event; Minh map được action.

Kết luận của nhóm: **CHƯA QUYẾT ĐỊNH**.

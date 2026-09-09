# Đào Văn Đức — checklist mô phỏng cho Factory Tour 11/09/2026

Trạng thái: biểu mẫu chuẩn bị, chưa chứa quan sát nhà máy. Đọc cùng checklist
của Minh tại `minh_optimization_questions.md`. Dùng `factory_tour/` theo cấu trúc
repo hiện có; kế hoạch cá nhân trước đó viết `factory-tour/`.

## Trước khi đi

- [ ] Kiểm tra thư mời và ca tham quan của nhóm với Minh.
- [ ] Mang bản checklist, giấy vẽ, bút và phương tiện bấm giờ nếu được phép.
- [ ] Thống nhất ID: N01… cho node, R01… cho resource, E01… cho route.
- [ ] Chọn cách ghi đơn vị: thời gian gốc phải được giữ nguyên; quy đổi sau.
- [ ] Đánh dấu các giả định chưa chốt trong `../simulation/des_mvp_decisions.md`.
- [ ] Đọc trước các câu hỏi về action/KPI của Minh để tránh hỏi trùng.

## Mười câu hỏi ưu tiên

| # | Hỏi/quan sát | Phải ghi được | Nếu chưa lấy được |
|---|---|---|---|
| 1 | Một đơn vị vật tư bắt đầu ở đâu, kết thúc ở đâu? | Entity và ranh giới lát cắt | Vẽ phần nhìn thấy, đánh dấu phần khuất |
| 2 | Vật tư đi qua những điểm nào, có nhánh/rework không? | Node ID, cạnh có hướng, route thay thế | Ghi UNKNOWN, không nối cạnh theo phỏng đoán |
| 3 | Vận chuyển từng box, pallet hay theo chuyến nhiều box? | Đơn vị job, batch size, sức chở | Giữ đơn vị gốc, chưa quy đổi sang jobs/hour |
| 4 | Những tuyến nào dùng chung xe/người? | Resource ID, số lượng, route được phục vụ | Không coi mọi xe cùng loại là một pool |
| 5 | Một lượt xe mất bao lâu từ lúc nhận yêu cầu đến sẵn sàng lại? | Chờ, load, đi, unload, quay về; n mẫu | Lấy khoảng ước lượng, ghi rõ phần đã bao gồm |
| 6 | Buffer đầy thì xe/người/máy làm gì? | Ai giữ vật tư, ai vẫn bị chiếm dụng, có mất hàng không | Đây là điểm phải chốt trước khi code blocking |
| 7 | Buffer rỗng thì công đoạn nào ngừng? | Starvation và trigger cấp bù | Hỏi tình huống cụ thể thay vì đoán từ ảnh |
| 8 | Chọn việc tiếp theo theo FIFO hay ưu tiên gì? | Quy tắc dispatch, ngoại lệ, người quyết định | Tạm để policy UNKNOWN |
| 9 | Một lần tắc gần đây đã xảy ra như thế nào? | Thời điểm, hàng đợi, nguyên nhân được kể, xử lý và kết quả | Tách lời kể khỏi giả thuyết của nhóm |
| 10 | Thay đổi nào thực sự làm được và KPI nào chứng minh hiệu quả? | Ít nhất 2 what-if, thời gian tác dụng, giới hạn | Chuyển Minh chốt tính khả thi và ưu tiên KPI |

Đồng Minh Đức phụ trách dự báo: bạn phối hợp ghi demand theo thời điểm, lịch ca
và lúc phát sinh yêu cầu. Khánh cần dữ liệu có ID/đơn vị/nguồn; Hưng cần sơ đồ
và API có thể nối được. Bạn sở hữu cơ chế vận động của hệ thống.

## Phiếu A — node, route, resource, buffer

**Khu vực / thời điểm / người ghi:** …

| Node ID | Vai trò | Entity vào → ra | Công đoạn/thao tác | Input/output buffer | Quan sát hay chưa biết |
|---|---|---|---|---|---|
| N01 | … | … | … | … | … |
| N02 | … | … | … | … | … |
| N03 | … | … | … | … | … |
| N04 | … | … | … | … | … |
| N05 | … | … | … | … | … |

| Route ID | From → To | Resource/pool | Một hay hai chiều | Batch/sức chở | Có tuyến thay thế? |
|---|---|---|---|---|---|
| E01 | … | … | … | … | … |

| Resource ID | Loại và số lượng | Dùng chung ở đâu | Khi nào bắt đầu bị chiếm dụng? | Khi nào giải phóng? | Lịch nghỉ/giới hạn |
|---|---|---|---|---|---|
| R01 | … | … | … | … | … |

| Buffer ID | Node | Đơn vị sức chứa | Capacity | Mức thấy được | Đầy thì sao? | Rỗng thì sao? | FIFO/priority |
|---|---|---|---|---|---|---|---|
| B01 | … | … | … | … | … | … | … |

Vẽ tay ngay sau mỗi khu vực. Nếu flow lớn, chỉ chọn một lát cắt gắn với một
pain point. Mục tiêu 5 node là yêu cầu khảo sát của nhóm; không bịa thêm node
hoặc shared resource để đủ số lượng.

## Phiếu B — đo/ước lượng tham số

Mỗi tham số dùng một phiếu; tối thiểu cố lấy 5 khoảng có cơ sở.

```text
Parameter ID / node / route / resource:
Đại lượng:
Định nghĩa điểm bắt đầu và điểm kết thúc:
Giá trị từng mẫu, theo đơn vị gốc:
Số mẫu n:
Khoảng [min, max] hoặc giá trị được cung cấp:
Khoảng này là min–max mẫu, ước lượng của người vận hành hay khoảng tin cậy?:
Đơn vị:
Ngày, giờ, ca và điều kiện tải:
Nguồn cụ thể / vai trò người cung cấp:
Nhãn: observed / estimated / assumed
Confidence: high / medium / low — vì sao:
Cần mentor xác thực?:
Ghi chú: đã bao gồm load/unload/đường về/chờ chưa?:
Có được phép đưa bản tổng hợp vào repo/proposal không?:
```

Không suy ra phân phối uniform/normal từ một khoảng min–max. Ba lần bấm giờ
chỉ là ba mẫu ở điều kiện đó. `observed` không đồng nghĩa với giá trị chuẩn
của mọi ca. Sau khi mentor xác thực, ghi nguồn/ngày xác thực và nhờ Khánh
map sang provenance chuẩn của repo; biểu mẫu này chưa thay đổi schema chung.

## Phiếu C — event và hàng đợi

| Event | Điều kiện kích hoạt | Entity đổi trạng thái | Resource chiếm/nhả | Queue/buffer tăng/giảm | Timestamp có thể ghi? |
|---|---|---|---|---|---|
| Vật tư được yêu cầu | … | … | … | … | … |
| Bắt đầu vận chuyển | … | … | … | … | … |
| Đến nơi/chờ dỡ | … | … | … | … | … |
| Bắt đầu/kết thúc xử lý | … | … | … | … | … |

Các tên trên là gợi ý quan sát, không khẳng định chúng tồn tại trong flow thật.

## Phiếu D — bottleneck hypothesis và what-if

```text
Giả thuyết tắc tại:
Dấu hiệu quan sát (queue/time/blocking/starvation):
Điều kiện tải/ca:
Giải thích khác cũng có thể đúng:
Phép kiểm tra để phân biệt các giải thích:
What-if 1 — thay gì, tại đâu, giữ gì cố định, giới hạn, khi nào tác dụng:
What-if 2 — thay gì, tại đâu, giữ gì cố định, giới hạn, khi nào tác dụng:
KPI cần so với hiện trạng:
Ai xác nhận action thực hiện được / nội dung còn UNKNOWN:
```

## Trước khi rời nhà máy

- [ ] Process map một lát cắt; mục tiêu ≥5 node nếu flow cho phép.
- [ ] Xác định ≥1 resource dùng chung và phạm vi dùng chung, hoặc ghi rõ chưa xác nhận.
- [ ] Xác định ≥1 buffer/queue và hành vi khi đầy/rỗng.
- [ ] ≥5 parameter range, kèm đơn vị, n/nguồn và confidence.
- [ ] ≥1 bottleneck hypothesis có bằng chứng và cách kiểm tra.
- [ ] ≥2 what-if có thể trao đổi với Minh về tính khả thi.
- [ ] Liệt kê 3 unknown quan trọng nhất cần mentor trả lời.

## Debrief tối 11/09

1. Chuyển sơ đồ vào `../domain/denso_process_map_v0.1.md`.
2. Tách ghi nhận thực tế, ước lượng, giả định và unknown; giữ ghi chú gốc riêng.
3. Nhờ Khánh nối ID/đơn vị/provenance; nhờ Minh nối action/constraint/KPI.
4. Nhờ Hưng đọc lại và diễn giải flow để kiểm tra tài liệu có đủ rõ.
5. Ghi những giả định MVP phải đổi sau chuyến đi và việc cần xong trước 14/09.

Ghi chú thô và dữ liệu chưa được phép chia sẻ ở ngoài Git, theo quy tắc repo.
File này chỉ là biểu mẫu trống để nhóm sử dụng.

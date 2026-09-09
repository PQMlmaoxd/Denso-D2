# DES MVP — đề xuất để Đào chốt trước khi triển khai

Trạng thái: **đề xuất kỹ thuật, chưa phải semantics được duyệt, chưa phải mô
hình DENSO**. Theo `AGENTS.md` §14, chủ module phải chốt simulation semantics
và KPI. Tài liệu quyết định chuẩn của team vẫn là
`../decision/modeling/d2_decision_model.tex`; không định nghĩa lại feasibility,
objective hoặc action catalog tại đây.

## 1. Những gì yêu cầu cá nhân đã chốt

- Toy flow: Warehouse → Transport → Buffer → Station A → Station B.
- Có arrivals, resource vận chuyển, finite buffer, processing time, queue và shared resource.
- Toàn bộ tham số từ config; cùng config và seed phải tái lập.
- KPI gồm throughput, lead time, WIP, queue length, utilization.
- Thử demand +10/20%, thời gian vận chuyển, buffer capacity, resource count, processing time.
- Có command/API trả JSON và so sánh baseline; ít nhất 3 scenario test.

Đó là phạm vi được giao. Các mục dưới đây là lựa chọn còn thiếu để code có
hành vi rõ ràng và test có đáp án thật.

## 2. Gói MVP A được đề xuất

| Quyết định | Đề xuất A cho toy synthetic | Lựa chọn khác / câu hỏi cần chốt |
|---|---|---|
| Entity | 1 job là 1 đơn vị vật tư giả lập; một chuyến chở 1 job | Nếu thực tế chở batch, cần entity/batch riêng |
| Đơn vị, horizon, initial state | Nội bộ dùng phút; horizon từ config; bắt đầu rỗng; xử lý event tại t ≤ T | Mô hình vận hành thật cần WIP/resource state đầu kỳ |
| Arrival | Bản kiểm tra dùng interarrival cố định; bản stochastic tùy chọn exponential theo rate từ config | Không suy ra Poisson từ quan sát ít mẫu; tour xác nhận schedule/batch/request |
| Queue | FIFO; không mất job; hàng chờ kho không giới hạn trong toy | Priority/cancellation/finite warehouse cần quy tắc riêng |
| Buffer | Capacity chỉ đếm job đang chờ trước A, không đếm job A đang xử lý | Nếu sức chứa tính cả in-service thì công thức trạng thái phải đổi |
| Blocking | Xe tới buffer đầy giữ job và giữ xe cho đến khi giao được | Có thể đặt hàng ở vùng chờ khác rồi giải phóng xe; phải hỏi tại tour |
| Shared resource | Một pool xe phục vụ các job; A/B có capacity riêng từ config | Chưa mô tả pool chung cho nhiều line/route khi chưa biết topology |
| Kết thúc horizon | Không drain sau T; job chưa xong vẫn là WIP; báo số hoàn thành và chưa xong | Có thể chọn drain, nhưng phải tách thời gian drain khỏi throughput trong kỳ |

Chưa có đơn vị cost và due date thì không gán cost/lateness bằng 0. Chưa chọn
adapter C++ cho đến khi Minh/Hưng chốt cách biểu diễn unavailable, vì C++ có
`cost` optional nhưng `lateness` là số thường. Thêm xe ở t=0 cho thí nghiệm
synthetic chưa tương đương action tạm thời có delay/window của decision core.

## 3. KPI đề xuất cho gói A

Các công thức sau chỉ là đề xuất đo trên toy, không chọn ưu tiên tối ưu.

| KPI | Định nghĩa đề xuất | Giới hạn phải báo |
|---|---|---|
| completed_jobs | Số job hoàn thành B trong [0,T] | T và đơn vị job phải đi kèm |
| throughput | 60 × completed_jobs / T, jobs/hour | Không nhầm số lượng với tốc độ |
| lead_time | Mean(completion − arrival) trên job đã hoàn thành trong kỳ | Nếu chưa job nào xong thì unavailable; có thiên lệch do job chưa xong |
| WIP | Tích phân số job đã đến nhưng chưa hoàn thành, chia T | Bao gồm kho, xe, buffer, service, hàng chờ B; báo thêm WIP cuối kỳ |
| Queue length | Tích phân số job chờ tại từng queue, chia T; báo thêm max | Không lấy snapshot cuối kỳ làm trung bình |
| Resource utilization | Tổng resource-time bị chiếm dụng / (capacity × T) | Tách active time và blocked time; utilization cao không tự chứng minh bottleneck |

Nếu chủ module chọn cách tính khác, sửa tiêu chí kiểm tra trước khi viết engine.
Giữ nguyên `shared/SimulationResult` cho đến khi chủ sở hữu liên quan duyệt
các trường thiếu, đặc biệt unavailable lead time, unit và run metadata.

## 4. Event cần có trong bản A

| Event | Thay đổi trạng thái | Ràng buộc phải giữ |
|---|---|---|
| Arrival | Tạo job, vào hàng chờ kho | ID duy nhất, timestamp không giảm |
| Transport start | Lấy job, chiếm một xe | Busy count không vượt capacity |
| Transport end | Thử giao job vào buffer/A | Đầy thì giữ xe; không mất job |
| Buffer release / A start | A lấy job, buffer có chỗ | Đánh thức xe bị block, không nhân đôi job |
| A end | Job rời A, đến hàng chờ B | Số job được bảo toàn |
| B start / B end | Chiếm/nhả B, ghi completion | Completion không trước arrival |

Thứ tự các event cùng timestamp cần cố định và được test. Hàng chờ B chưa có
capacity vật lý xác nhận; đề xuất A dùng hàng chờ không giới hạn và ghi rõ
giới hạn này, không suy rộng thành nhà máy thật.

## 5. Acceptance cases trước khi đánh dấu Toy DES hoàn thành

| Case | Dữ liệu kiểm tra | Điều cần chứng minh |
|---|---|---|
| Hand calculation | Vài job, arrivals và service time cố định | Timestamp từng event/completion bằng tính tay |
| Resource contention | ≥2 job cần xe trong cùng khoảng | Không có xe phục vụ đồng thời quá capacity |
| Finite buffer | Buffer nhỏ, A chậm | Queue ≤ capacity; xe bị block đúng quy tắc đã chọn |
| Conservation | Nhiều mức tải, kể cả quá tải | Arrived = completed + unfinished tại T |
| Low load / zero demand | Không có hoặc rất ít job | Không chia 0; utilization/WIP hợp lệ; lead time unavailable nếu cần |
| Horizon boundary | Event ở trước, đúng và sau T | Cutoff nhất quán, không tính thời gian sau T vào KPI |
| Replay | Cùng config + seed | Kết quả và event log trùng khớp |
| Scenario effect | Demand +10/+20%, count/time/buffer thay đổi | Input thực sự đi vào dynamics; giải thích plateau nếu có |

Không yêu cầu tăng buffer hay xe luôn làm mọi KPI tốt hơn. Kiểm tra chiều tác
động trong instance có bottleneck đã biết, không dùng khẳng định tổng quát.

## 6. Bàn giao cho từng người

| Người | Bạn cần nhận | Bạn giao lại |
|---|---|---|
| Khánh | Config có ID/units/provenance và kiểm tra dữ liệu | Danh sách tham số; entity/event quan sát được |
| Đồng Minh Đức | Ý nghĩa target, rate/count, bước thời gian, horizon, scenario | Trạng thái nào simulator cần; cảnh báo trùng nguồn uncertainty |
| Minh | Action payload, giới hạn, delay/window, baseline no-action | KPI từ cùng simulator cho base/candidate, scenario/seed rõ |
| Hưng | Cách gọi/format đã thống nhất | API chạy độc lập, fixture, log, runtime và lỗi dễ tái lập |

Boundary đích theo tài liệu team: `(state, action, scenario, seed) → KPI result`.
Python hiện tại chỉ nhận `(config, forecast)`; C++ có `SimulationEvaluator`.
Chưa có binding nối hai phần. Đây là công việc phối hợp, không thể hoàn thành
bằng cách chỉ thêm một hàm Python tên `evaluate`.

## 7. Quyết định cần ghi lại

- Người chốt: …
- Ngày: …
- Gói A được chấp nhận cho toy synthetic, hay mục cần đổi: …
- KPI/units/unavailable đã được các bên liên quan xác nhận: …
- Giả định bắt buộc kiểm chứng trong Factory Tour: …

Sau khi chốt, đầu việc triển khai đầu tiên là event engine + hand-calculation
test; tiếp theo resource/buffer; cuối cùng scenario runner và adapter. Không
thay toàn bộ pipeline trong một PR.

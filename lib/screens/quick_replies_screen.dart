import 'package:flutter/material.dart';

class QuickRepliesScreen extends StatefulWidget {
  const QuickRepliesScreen({super.key});

  @override
  State<QuickRepliesScreen> createState() => _QuickRepliesScreenState();
}

class _QuickRepliesScreenState extends State<QuickRepliesScreen> {
  // Danh sách các câu trả lời nhanh mẫu bằng tiếng Việt (Trùng khớp với thiết kế cứng trên đồng hồ làm mặc định)
  final List<String> _replies = [
    "OK",
    "Tôi đang bận",
    "Đang lái xe, gọi lại sau",
    "Đồng ý",
    "Không đồng ý",
    "Đã nhận thông tin",
    "Gọi lại cho tôi nhé",
  ];

  final TextEditingController _textController = TextEditingController();

  void _addReply() {
    String txt = _textController.text.trim();
    if (txt.isNotEmpty) {
      setState(() {
        _replies.add(txt);
        _textController.clear();
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: const Text('Thêm câu trả lời nhanh thành công!'),
          backgroundColor: Colors.indigo.shade800,
        ),
      );
    }
  }

  void _deleteReply(int index) {
    setState(() {
      _replies.removeAt(index);
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0F172A),
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [Color(0xFF0F172A), Color(0xFF0F172A), Color(0xFF020617)],
          ),
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(20.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'NOTIF INTERACTION',
                  style: TextStyle(
                    color: Colors.white38,
                    fontSize: 12,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 1.5,
                  ),
                ),
                const SizedBox(height: 4),
                const Text(
                  'Mẫu tin trả lời nhanh',
                  style: TextStyle(
                    color: Colors.white,
                    fontSize: 22,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  'Các câu trả lời mẫu hiển thị trên đồng hồ để phản hồi tin nhắn ngầm lập tức.',
                  style: TextStyle(
                    color: Colors.white.withValues(alpha: 0.4),
                    fontSize: 13,
                  ),
                ),
                const SizedBox(height: 24),

                // ── BỘ THÊM CÂU MỚI ──
                Container(
                  padding: const EdgeInsets.all(16),
                  decoration: BoxDecoration(
                    color: Colors.white.withValues(alpha: 0.02),
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(
                      color: Colors.white.withValues(alpha: 0.05),
                    ),
                  ),
                  child: Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _textController,
                          style: const TextStyle(color: Colors.white),
                          decoration: InputDecoration(
                            hintText: 'Thêm mẫu trả lời mới...',
                            hintStyle: const TextStyle(color: Colors.white24),
                            border: InputBorder.none,
                            contentPadding: const EdgeInsets.symmetric(
                              horizontal: 10,
                            ),
                          ),
                        ),
                      ),
                      IconButton(
                        onPressed: _addReply,
                        icon: const Icon(
                          Icons.add_circle,
                          color: Color(0xFF3B82F6),
                          size: 30,
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 20),

                // ── DANH SÁCH CÂU HỎI NHANH ──
                Expanded(
                  child: ListView.builder(
                    itemCount: _replies.length,
                    physics: const BouncingScrollPhysics(),
                    itemBuilder: (context, index) {
                      return Container(
                        margin: const EdgeInsets.only(bottom: 12),
                        padding: const EdgeInsets.symmetric(
                          horizontal: 20,
                          vertical: 16,
                        ),
                        decoration: BoxDecoration(
                          color: Colors.white.withValues(alpha: 0.02),
                          borderRadius: BorderRadius.circular(16),
                          border: Border.all(
                            color: Colors.white.withValues(alpha: 0.04),
                          ),
                        ),
                        child: Row(
                          children: [
                            Container(
                              width: 28,
                              height: 28,
                              decoration: BoxDecoration(
                                color: const Color(
                                  0xFF3B82F6,
                                ).withValues(alpha: 0.1),
                                shape: BoxShape.circle,
                              ),
                              child: Center(
                                child: Text(
                                  '${index + 1}',
                                  style: const TextStyle(
                                    color: Color(0xFF3B82F6),
                                    fontSize: 13,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ),
                            ),
                            const SizedBox(width: 16),
                            Expanded(
                              child: Text(
                                _replies[index],
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontSize: 15,
                                  fontWeight: FontWeight.w500,
                                ),
                              ),
                            ),
                            IconButton(
                              onPressed: () => _deleteReply(index),
                              icon: const Icon(
                                Icons.delete_outline_rounded,
                                color: Colors.white30,
                              ),
                            ),
                          ],
                        ),
                      );
                    },
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

import { motion } from 'framer-motion';
import { Network, Database, Zap, Cpu, TrendingUp, Shield, Send } from 'lucide-react';

export default function Architecture() {
  const pipeline = [
    { icon: Network, name: 'Market Data', latency: '87ns', color: '#3b82f6' },
    { icon: Database, name: 'Order Book', latency: '50ns', color: '#10b981' },
    { icon: Zap, name: 'Signal Processing', latency: '150ns', color: '#f59e0b' },
    { icon: Cpu, name: 'ML Inference', latency: '400ns', color: '#8b5cf6' },
    { icon: TrendingUp, name: 'Strategy', latency: '200ns', color: '#ec4899' },
    { icon: Shield, name: 'Risk Check', latency: '10ns', color: '#ef4444' },
    { icon: Send, name: 'Order Send', latency: '34ns', color: '#06b6d4' },
  ];

  return (
    <section id="architecture" className="relative py-32 px-4 sm:px-6 lg:px-8 bg-gradient-to-b from-white to-secondary/50">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="text-center mb-20"
        >
          <h2 className="text-5xl md:text-6xl font-light text-foreground mb-6">
            System <span className="font-bold">Pipeline</span>
          </h2>
          <p className="text-xl text-muted max-w-3xl mx-auto">
            Seven-stage execution pipeline achieving consistent sub-microsecond latency
          </p>
        </motion.div>

        {/* Horizontal Pipeline */}
        <div className="mb-20">
          <div className="relative">
            {/* Desktop: Horizontal Flow */}
            <div className="hidden lg:block">
              <div className="flex items-center justify-between gap-2 mb-8">
                {pipeline.map((stage, i) => (
                  <motion.div
                    key={i}
                    initial={{ opacity: 0, y: 20 }}
                    whileInView={{ opacity: 1, y: 0 }}
                    viewport={{ once: true }}
                    transition={{ delay: i * 0.1 }}
                    className="flex-1"
                  >
                    <div className="relative">
                      <div className="p-6 rounded-2xl bg-white border-2 border-border hover:border-foreground/30 hover:shadow-lg transition-all group">
                        <div 
                          className="w-14 h-14 rounded-xl flex items-center justify-center mx-auto mb-4"
                          style={{ backgroundColor: `${stage.color}15` }}
                        >
                          <stage.icon className="w-7 h-7" style={{ color: stage.color }} />
                        </div>
                        <h3 className="text-sm font-semibold text-foreground text-center mb-2">{stage.name}</h3>
                        <div className="text-2xl font-mono font-bold text-center" style={{ color: stage.color }}>
                          {stage.latency}
                        </div>
                      </div>
                      
                      {/* Arrow */}
                      {i < pipeline.length - 1 && (
                        <div className="absolute top-1/2 -right-2 transform -translate-y-1/2 z-10">
                          <div className="w-4 h-0.5 bg-border"></div>
                          <div className="absolute right-0 top-1/2 transform -translate-y-1/2">
                            <div className="w-2 h-2 border-t-2 border-r-2 border-border rotate-45 transform translate-x-1"></div>
                          </div>
                        </div>
                      )}
                    </div>
                  </motion.div>
                ))}
              </div>
            </div>

            {/* Mobile: Vertical Flow */}
            <div className="lg:hidden space-y-4">
              {pipeline.map((stage, i) => (
                <motion.div
                  key={i}
                  initial={{ opacity: 0, x: -20 }}
                  whileInView={{ opacity: 1, x: 0 }}
                  viewport={{ once: true }}
                  transition={{ delay: i * 0.1 }}
                >
                  <div className="p-6 rounded-2xl bg-white border-2 border-border">
                    <div className="flex items-center gap-4">
                      <div 
                        className="w-12 h-12 rounded-xl flex items-center justify-center flex-shrink-0"
                        style={{ backgroundColor: `${stage.color}15` }}
                      >
                        <stage.icon className="w-6 h-6" style={{ color: stage.color }} />
                      </div>
                      <div className="flex-1">
                        <h3 className="text-base font-semibold text-foreground">{stage.name}</h3>
                      </div>
                      <div className="text-2xl font-mono font-bold" style={{ color: stage.color }}>
                        {stage.latency}
                      </div>
                    </div>
                  </div>
                </motion.div>
              ))}
            </div>
          </div>
        </div>

        {/* Stats Cards */}
        <div className="grid md:grid-cols-3 gap-8 mb-20">
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            className="p-8 rounded-2xl bg-secondary border-2 border-border"
          >
            <div className="text-5xl font-bold mb-3 text-foreground">890 ns</div>
            <div className="text-lg mb-2 text-foreground">Total Latency</div>
            <div className="text-sm text-muted">p99: 921ns • p99.9: 1,047ns</div>
          </motion.div>

          <motion.div
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            transition={{ delay: 0.1 }}
            className="p-8 rounded-2xl bg-white border-2 border-border"
          >
            <div className="text-5xl font-bold mb-3 text-foreground">100%</div>
            <div className="text-lg mb-2 text-foreground">Deterministic</div>
            <div className="text-sm text-muted">Bit-identical replay with SHA-256 verification</div>
          </motion.div>

          <motion.div
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            transition={{ delay: 0.2 }}
            className="p-8 rounded-2xl bg-white border-2 border-border"
          >
            <div className="text-5xl font-bold mb-3 text-foreground">0</div>
            <div className="text-lg mb-2 text-foreground">Locks Used</div>
            <div className="text-sm text-muted">Lock-free SPSC/MPSC queues throughout</div>
          </motion.div>
        </div>

        {/* Technical Details */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="grid md:grid-cols-2 gap-8"
        >
          <div className="p-8 rounded-2xl bg-white border-2 border-border">
            <h3 className="text-2xl font-bold text-foreground mb-6">Performance Optimizations</h3>
            <div className="space-y-4">
              {[
                { label: 'Zero-Copy Operations', value: 'DMA transfers, shared memory' },
                { label: 'Cache Optimization', value: '64-byte alignment, SoA structures' },
                { label: 'SIMD Processing', value: 'AVX-512 vectorized operations' },
                { label: 'Memory Pre-allocation', value: 'No malloc on hot path' },
              ].map((item, i) => (
                <div key={i} className="flex justify-between items-start pb-4 border-b border-border last:border-0">
                  <span className="font-semibold text-foreground">{item.label}</span>
                  <span className="text-muted text-sm text-right">{item.value}</span>
                </div>
              ))}
            </div>
          </div>

          <div className="p-8 rounded-2xl bg-white border-2 border-border">
            <h3 className="text-2xl font-bold text-foreground mb-6">System Guarantees</h3>
            <div className="space-y-4">
              {[
                { label: 'Latency Consistency', value: 'p99.9 < 1.2μs guaranteed' },
                { label: 'Order Processing', value: 'First-In-First-Out execution' },
                { label: 'Risk Controls', value: 'Atomic pre-trade validation' },
                { label: 'Fault Tolerance', value: 'Graceful degradation modes' },
              ].map((item, i) => (
                <div key={i} className="flex justify-between items-start pb-4 border-b border-border last:border-0">
                  <span className="font-semibold text-foreground">{item.label}</span>
                  <span className="text-muted text-sm text-right">{item.value}</span>
                </div>
              ))}
            </div>
          </div>
        </motion.div>
      </div>
    </section>
  );
}

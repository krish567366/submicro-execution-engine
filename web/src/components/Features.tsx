import { motion } from 'framer-motion';
import { Zap, Lock, Cpu, BarChart3, Shield, Clock } from 'lucide-react';

export default function Features() {
  const features = [
    {
      icon: Zap,
      title: 'Lock-Free Architecture',
      description: 'SPSC/MPSC queues with zero contention. No mutexes, no blocking, pure speed.',
      color: '#3b82f6',
    },
    {
      icon: Clock,
      title: 'Deterministic Execution',
      description: 'Bit-identical replay capability with SHA-256 verification and fixed RNG seeds.',
      color: '#10b981',
    },
    {
      icon: Cpu,
      title: 'SIMD Optimized',
      description: 'AVX-512 vectorization for parallel processing across all hot paths.',
      color: '#8b5cf6',
    },
    {
      icon: BarChart3,
      title: 'Real-Time Analytics',
      description: 'Live performance metrics, order book depth, and execution quality monitoring.',
      color: '#f59e0b',
    },
    {
      icon: Shield,
      title: 'Risk Management',
      description: 'Atomic pre-trade checks with configurable position limits and notional caps.',
      color: '#ef4444',
    },
    {
      icon: Lock,
      title: 'Production Ready',
      description: 'Battle-tested with comprehensive logging, monitoring, and graceful degradation.',
      color: '#ec4899',
    },
  ];

  return (
    <section className="relative py-32 px-4 sm:px-6 lg:px-8 bg-white">
      <div className="max-w-7xl mx-auto">
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="text-center mb-20"
        >
          <h2 className="text-5xl md:text-6xl font-light text-foreground mb-6">
            Core <span className="font-bold">Features</span>
          </h2>
          <p className="text-xl text-muted max-w-3xl mx-auto">
            Built from the ground up for institutional-grade performance
          </p>
        </motion.div>

        <div className="grid md:grid-cols-2 lg:grid-cols-3 gap-8">
          {features.map((feature, index) => (
            <motion.div
              key={index}
              initial={{ opacity: 0, y: 20 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true }}
              transition={{ delay: index * 0.1 }}
              className="group"
            >
              <div className="p-8 rounded-2xl bg-white border-2 border-border hover:border-foreground/30 hover:shadow-xl transition-all h-full">
                <div 
                  className="w-14 h-14 rounded-xl flex items-center justify-center mb-6"
                  style={{ backgroundColor: `${feature.color}15` }}
                >
                  <feature.icon className="w-7 h-7" style={{ color: feature.color }} />
                </div>
                <h3 className="text-xl font-bold text-foreground mb-3">{feature.title}</h3>
                <p className="text-muted leading-relaxed">{feature.description}</p>
              </div>
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}

import { motion } from 'framer-motion';
import { Code2, Cpu, Gauge, GitBranch } from 'lucide-react';

const techStack = [
  {
    category: 'Core',
    icon: Cpu,
    color: '#3b82f6',
    technologies: [
      { name: 'C++17', desc: 'High-performance execution' },
      { name: 'Rust', desc: 'Memory-safe components' },
      { name: 'CMake', desc: 'Build system' },
    ],
  },
  {
    category: 'Optimization',
    icon: Gauge,
    color: '#8b5cf6',
    technologies: [
      { name: 'SIMD AVX-512', desc: 'Vectorized operations' },
      { name: 'Lock-Free Structures', desc: 'SPSC/MPSC queues' },
      { name: 'Zero-Copy I/O', desc: 'DMA transfers' },
    ],
  },
  {
    category: 'Infrastructure',
    icon: GitBranch,
    color: '#10b981',
    technologies: [
      { name: 'DPDK (Simulated)', desc: 'Kernel bypass' },
      { name: 'TSC + PTP', desc: 'Nanosecond timing' },
      { name: 'Huge Pages', desc: 'TLB optimization' },
    ],
  },
  {
    category: 'Algorithms',
    icon: Code2,
    color: '#f59e0b',
    technologies: [
      { name: 'Hawkes Process', desc: 'Order flow modeling' },
      { name: 'Avellaneda-Stoikov', desc: 'Market making' },
      { name: 'DNN Inference', desc: 'Signal extraction' },
    ],
  },
];

export default function TechStack() {
  return (
    <section className="relative py-20 px-4 sm:px-6 lg:px-8 bg-white">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="text-center mb-16"
        >
          <h2 className="text-4xl md:text-5xl font-light text-foreground mb-4">
            Technology <span className="font-semibold">Stack</span>
          </h2>
          <p className="text-lg text-muted max-w-3xl mx-auto leading-relaxed">
            Built with cutting-edge technologies for maximum performance and reliability.
          </p>
        </motion.div>

        {/* Tech Stack Grid */}
        <div className="grid md:grid-cols-2 lg:grid-cols-4 gap-8">
          {techStack.map((category, idx) => (
            <motion.div
              key={idx}
              initial={{ opacity: 0, y: 20 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true }}
              transition={{ delay: idx * 0.1 }}
              className="group"
            >
              <div className="h-full p-6 rounded-lg bg-white border border-border hover:border-foreground/20 transition-all duration-300">
                {/* Icon */}
                <div className="w-12 h-12 rounded-lg flex items-center justify-center mb-4 group-hover:scale-105 transition-transform duration-300" style={{ backgroundColor: `${category.color}15` }}>
                  <category.icon className="w-6 h-6" style={{ color: category.color }} />
                </div>

                {/* Category */}
                <h3 className="text-xl font-medium text-foreground mb-4">{category.category}</h3>

                {/* Technologies */}
                <div className="space-y-3">
                  {category.technologies.map((tech, i) => (
                    <div key={i}>
                      <div className="text-sm font-medium text-foreground">
                        {tech.name}
                      </div>
                      <div className="text-xs text-muted">{tech.desc}</div>
                    </div>
                  ))}
                </div>
              </div>
            </motion.div>
          ))}
        </div>

        {/* Compiler Flags */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="mt-16 p-8 rounded-lg bg-secondary border border-border"
        >
          <h3 className="text-2xl font-medium text-foreground mb-4 flex items-center gap-2">
            <Code2 className="w-6 h-6 text-foreground" />
            Compiler Optimizations
          </h3>
          <div className="grid md:grid-cols-2 gap-4">
            <div className="font-mono text-sm">
              <div className="text-foreground font-medium">-O3</div>
              <div className="text-muted text-xs">Maximum optimization</div>
            </div>
            <div className="font-mono text-sm">
              <div className="text-foreground font-medium">-march=native -mtune=native</div>
              <div className="text-muted text-xs">CPU-specific instructions</div>
            </div>
            <div className="font-mono text-sm">
              <div className="text-foreground font-medium">-flto</div>
              <div className="text-muted text-xs">Link-time optimization</div>
            </div>
            <div className="font-mono text-sm">
              <div className="text-foreground font-medium">-ffast-math</div>
              <div className="text-muted text-xs">Fast floating-point</div>
            </div>
          </div>
        </motion.div>
      </div>
    </section>
  );
}

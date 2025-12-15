import { motion } from 'framer-motion';
import { Github, ExternalLink } from 'lucide-react';

export default function Hero() {
  return (
    <section className="relative min-h-screen flex items-center px-4 sm:px-6 lg:px-8 bg-white">
      <div className="max-w-7xl mx-auto w-full py-20">
        <div className="grid lg:grid-cols-2 gap-12 items-center">
          {/* Left Column - Text Content */}
          <motion.div
            initial={{ opacity: 0, x: -30 }}
            animate={{ opacity: 1, x: 0 }}
            transition={{ duration: 0.6 }}
            className="space-y-8"
          >
            <div className="space-y-4">
              <h1 className="text-5xl md:text-7xl font-light text-foreground leading-tight">
                Ultra-Low Latency
                <br />
                <span className="font-bold">Trading System</span>
              </h1>
              <p className="text-xl text-muted leading-relaxed">
                Sub-microsecond execution engine built with C++17 and Rust.
                Designed for institutional-grade algorithmic trading.
              </p>
            </div>

            <div className="flex flex-wrap gap-4">
              <a
                href="https://github.com/krish567366/submicro-execution-engine"
                target="_blank"
                rel="noopener noreferrer"
                className="inline-flex items-center gap-2 px-6 py-3 rounded-lg bg-secondary border-2 border-border text-foreground font-medium hover:border-foreground/50 transition-all"
              >
                <Github className="w-5 h-5" />
                View on GitHub
              </a>
              <a
                href="#architecture"
                className="inline-flex items-center gap-2 px-6 py-3 rounded-lg border-2 border-border text-foreground font-medium hover:border-foreground/50 transition-all"
              >
                <ExternalLink className="w-4 h-4" />
                Documentation
              </a>
            </div>

            <div className="grid grid-cols-3 gap-6 pt-4">
              {[
                { value: 'C++17', label: 'Core Language' },
                { value: 'Rust', label: 'Safe Components' },
                { value: 'SIMD', label: 'Optimizations' },
              ].map((item, i) => (
                <div key={i}>
                  <div className="text-2xl font-bold text-foreground">{item.value}</div>
                  <div className="text-sm text-muted mt-1">{item.label}</div>
                </div>
              ))}
            </div>
          </motion.div>

          {/* Right Column - Stats Cards */}
          <motion.div
            initial={{ opacity: 0, x: 30 }}
            animate={{ opacity: 1, x: 0 }}
            transition={{ duration: 0.6, delay: 0.2 }}
            className="space-y-4"
          >
            {/* Main Latency Card */}
            <div className="p-8 rounded-lg bg-secondary border-2 border-border">
              <div className="text-sm mb-2 text-muted">Median Latency</div>
              <div className="text-6xl font-bold mb-3 text-foreground">890 ns</div>
              <div className="text-sm text-muted">p99: 921ns • p99.9: 1,047ns</div>
            </div>

            {/* Stats Grid */}
            <div className="grid grid-cols-2 gap-4">
              {[
                { value: '100%', label: 'Deterministic' },
                { value: '0', label: 'Locks Used' },
                { value: '64B', label: 'Cache Aligned' },
                { value: '<1μs', label: 'End-to-End' },
              ].map((stat, i) => (
                <div key={i} className="p-6 rounded-lg bg-secondary border border-border">
                  <div className="text-3xl font-bold text-foreground mb-2">{stat.value}</div>
                  <div className="text-sm text-muted">{stat.label}</div>
                </div>
              ))}
            </div>
          </motion.div>
        </div>
      </div>
    </section>
  );
}

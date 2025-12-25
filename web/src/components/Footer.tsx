import { motion } from 'framer-motion';
import { Github, Twitter, Linkedin, Mail, Star, Heart } from 'lucide-react';

export default function Footer() {
  return (
    <footer className="relative py-16 px-4 sm:px-6 lg:px-8 bg-secondary border-t border-border">
      <div className="max-w-7xl mx-auto">
        {/* Main Footer Content */}
        <div className="grid md:grid-cols-4 gap-12 mb-12">
          {/* Brand */}
          <div className="md:col-span-2">
            <motion.div
              initial={{ opacity: 0, y: 20 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true }}
            >
              <h3 className="text-2xl font-semibold text-foreground mb-4">SubMicro Engine</h3>
              <p className="text-muted mb-6 leading-relaxed">
                An open-source, ultra-low-latency trading execution engine built for quantitative research and systems engineering.
              </p>
              <div className="flex items-center gap-4">
                <a
                  href="https://github.com/krish567366/submicro-execution-engine"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg border border-border hover:border-foreground/20 text-muted hover:text-foreground transition-all"
                >
                  <Github className="w-5 h-5" />
                </a>
                <a
                  href="mailto:krishna@krishnabajpai.me"
                  className="p-2 rounded-lg border border-border hover:border-foreground/20 text-muted hover:text-foreground transition-all"
                >
                  <Mail className="w-5 h-5" />
                </a>
                <a
                  href="https://twitter.com"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg border border-border hover:border-foreground/20 text-muted hover:text-foreground transition-all"
                >
                  <Twitter className="w-5 h-5" />
                </a>
                <a
                  href="https://linkedin.com"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="p-2 rounded-lg border border-border hover:border-foreground/20 text-muted hover:text-foreground transition-all"
                >
                  <Linkedin className="w-5 h-5" />
                </a>
              </div>
            </motion.div>
          </div>

          {/* Quick Links */}
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            transition={{ delay: 0.1 }}
          >
            <h4 className="text-foreground font-semibold mb-4">Quick Links</h4>
            <ul className="space-y-2">
              {['Documentation', 'Architecture', 'Performance', 'Live Demo'].map((link) => (
                <li key={link}>
                  <a
                    href={`#${link.toLowerCase().replace(' ', '-')}`}
                    className="text-muted hover:text-foreground transition-colors text-sm"
                  >
                    {link}
                  </a>
                </li>
              ))}
            </ul>
          </motion.div>

          {/* Resources */}
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            transition={{ delay: 0.2 }}
          >
            <h4 className="text-foreground font-semibold mb-4">Resources</h4>
            <ul className="space-y-2">
              {['GitHub Repository', 'API Reference', 'Contributing', 'License'].map((link) => (
                <li key={link}>
                  <a href="#" className="text-muted hover:text-foreground transition-colors text-sm">
                    {link}
                  </a>
                </li>
              ))}
            </ul>
          </motion.div>
        </div>

        {/* Divider */}
        <div className="border-t border-border pt-8">
          <div className="flex flex-col md:flex-row justify-between items-center gap-4">
            {/* Copyright */}
            <div className="text-muted text-sm">
              © 2025 SubMicro Engine. Research & Educational Use Only.
            </div>

            {/* Disclaimer */}
            <div className="flex items-center gap-2 text-xs">
              <span className="px-3 py-1 rounded-full bg-yellow-50 border border-yellow-200 text-yellow-700">
                 Not for Production Trading
              </span>
            </div>

            {/* Star on GitHub */}
            <a
              href="https://github.com/krish567366/submicro-execution-engine"
              target="_blank"
              rel="noopener noreferrer"
              className="flex items-center gap-2 px-4 py-2 rounded-lg border border-border hover:border-foreground/20 text-muted hover:text-foreground transition-all group"
            >
              <Star className="w-4 h-4 group-hover:fill-yellow-500 group-hover:text-yellow-500 transition-all" />
              <span className="text-sm font-medium">Star on GitHub</span>
            </a>

            {/* Sponsor on GitHub */}
            <a
              href="https://github.com/sponsors/krish567366"
              target="_blank"
              rel="noopener noreferrer"
              className="flex items-center gap-2 px-4 py-2 rounded-lg border border-red-200 bg-red-50 hover:border-red-300 hover:bg-red-100 text-red-700 transition-all group"
            >
              <Heart className="w-4 h-4 group-hover:fill-red-500 group-hover:text-red-500 transition-all" />
              <span className="text-sm font-medium">Sponsor</span>
            </a>
          </div>
        </div>

        {/* Tech Stack Badge */}
        <motion.div
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          transition={{ delay: 0.5 }}
          className="mt-8 text-center text-xs text-muted"
        >
          Built with React • TypeScript • Tailwind CSS • Framer Motion
        </motion.div>
      </div>
    </footer>
  );
}

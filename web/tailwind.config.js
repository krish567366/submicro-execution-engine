/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        background: '#ffffff',
        foreground: '#0a0a0a',
        card: '#ffffff',
        'card-foreground': '#0a0a0a',
        primary: '#000000',
        'primary-foreground': '#ffffff',
        secondary: '#fafafa',
        'secondary-foreground': '#0a0a0a',
        muted: '#6b7280',
        'muted-foreground': '#6b7280',
        accent: '#0a0a0a',
        'accent-foreground': '#ffffff',
        border: '#e5e7eb',
        // Chart colors - professional palette
        chart: {
          1: '#3b82f6', // blue
          2: '#10b981', // green
          3: '#f59e0b', // amber
          4: '#8b5cf6', // violet
          5: '#ec4899', // pink
        },
      },
      fontFamily: {
        body: ['Inter', 'system-ui', 'sans-serif'],
        sketch: ['Caveat', 'cursive'],
      },
      fontSize: {
        'xs': '0.75rem',
        'sm': '0.875rem',
        'base': '1rem',
        'lg': '1.125rem',
        'xl': '1.25rem',
        '2xl': '1.5rem',
        '3xl': '1.875rem',
        '4xl': '2.25rem',
        '5xl': '3rem',
        '6xl': '3.75rem',
        '7xl': '4.5rem',
      },
      animation: {
        'fade-in': 'fadeIn 0.6s ease-out',
        'slide-up': 'slideUp 0.6s ease-out',
      },
      keyframes: {
        fadeIn: {
          '0%': { opacity: '0' },
          '100%': { opacity: '1' },
        },
        slideUp: {
          '0%': { transform: 'translateY(20px)', opacity: '0' },
          '100%': { transform: 'translateY(0)', opacity: '1' },
        },
      },
    },
  },
  plugins: [],
}

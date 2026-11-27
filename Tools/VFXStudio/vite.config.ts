import { defineConfig } from 'vite';
export default defineConfig({
  build: { outDir: 'dist/client', emptyOutDir: false },
  server: { proxy: { '/api': 'http://127.0.0.1:4317' } },
});

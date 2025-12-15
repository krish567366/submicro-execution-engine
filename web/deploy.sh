#!/bin/bash

# Deployment script for SubMicro Engine Frontend
# Usage: ./deploy.sh your-subdomain.yourcompany.com

if [ -z "$1" ]; then
    echo "❌ Error: Please provide subdomain as argument"
    echo "Usage: ./deploy.sh your-subdomain.yourcompany.com"
    exit 1
fi

SUBDOMAIN=$1

echo "🚀 Building SubMicro Engine Frontend..."
npm run build

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "✅ Build successful!"
echo "📦 Production files are in ./dist/"
echo ""
echo "📋 Next steps to deploy to $SUBDOMAIN:"
echo "   1. Upload the contents of ./dist/ to your web server"
echo "   2. Configure your subdomain DNS to point to the server"
echo "   3. Setup HTTPS certificate (Let's Encrypt recommended)"
echo ""
echo "🔧 Example deployment commands:"
echo ""
echo "   # Using SCP to remote server:"
echo "   scp -r dist/* user@your-server:/var/www/$SUBDOMAIN/"
echo ""
echo "   # Using rsync:"
echo "   rsync -avz dist/ user@your-server:/var/www/$SUBDOMAIN/"
echo ""
echo "   # Using AWS S3 + CloudFront:"
echo "   aws s3 sync dist/ s3://your-bucket/ --delete"
echo "   aws cloudfront create-invalidation --distribution-id YOUR_ID --paths '/*'"
echo ""
echo "✨ Done!"

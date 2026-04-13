cat > ~/hm << 'EOF'
#!/bin/bash
echo "$1" > /tmp/hmdcam_input
EOF

chmod +x ~/hm
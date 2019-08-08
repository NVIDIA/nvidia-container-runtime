# Testing of nvidia-container-runtime/nvidia-docker2

## Running tests on AWS

Install Terraform
```sh
wget https://releases.hashicorp.com/terraform/0.12.6/terraform_0.12.6_linux_amd64.zip
unzip terraform_0.12.6_linux_amd64.zip
export PATH="$PATH:$(pwd)"
cd /usr/bin && sudo ln -s /path/to/terraform terraform
```

Generate ssh keys in keys folder and name public key "aws_terraform.pub" and private key "aws_terraform"
```sh
export AWS_ACCESS_KEY_ID="***"
export AWS_SECRET_ACCESS_KEY="***"
cd ubuntu16.04
terraform init && terraform apply && terraform destroy
cd centos7
terraform init && terraform apply && terraform destroy
```

## Running tests locally
```sh
sudo commons/bats_install.sh
sudo commons/run_tests.sh
```

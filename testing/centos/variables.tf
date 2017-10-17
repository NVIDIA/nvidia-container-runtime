variable "instance_name" {
	description = "Instance Name"
}

variable "instance_type" {
	 description = "Instance Type"
}

variable "ami" {
	 description = "nvidia-docker 2.0 test AMI based on Centos7 AMI"
}

variable "aws_region" {
	 default = "us-east-1"
	 description = "AMI Location in US"
}

variable "key_name" {
	 description = "Name of the SSH keypair to use in the AMI."
}

variable "privkey_path" {
	 description = "Path to the Private SSH key."
}

variable "pubkey_path" {
	 description = "Path to the Public SSH key."
}

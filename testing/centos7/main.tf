provider "aws" {
  region = "${var.aws_region}"
}

data "aws_ami" "centos7" {
  most_recent = true
  owners      = ["self"]
  name_regex  = "${var.ami_name}"
}

resource "aws_security_group" "allow_ssh" {
  name        = "ssh_centos7"
  description = "Allow ssh connections on port 22"

  ingress {
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

resource "aws_instance" "nvidiaDockerCentos" {
  ami           = "${data.aws_ami.centos7.id}"
  instance_type = "${var.instance_type}"

  tags = {
    Name = "${var.instance_name}"
  }

  root_block_device {
    volume_size = 80
  }

  key_name = "${var.key_name}"

  security_groups = ["${aws_security_group.allow_ssh.name}"]

  connection {
    host        = self.public_ip
    type        = "ssh"
    user        = "centos"
    private_key = "${file("${var.privkey_path}")}"
    agent       = false
    timeout     = "5m"
  }

  provisioner "file" {
    source      = "nvDocker2_install.sh"
    destination = "~/nvDocker2_install.sh"
  }

  provisioner "file" {
    source      = "../commons/bats_install.sh"
    destination = "~/bats_install.sh"
  }

  provisioner "local-exec" {
    command = "${var.artifacts == true ? "$SCP_CMD" : "echo not using build artifacts"}"
    environment = {
      SCP_CMD = "scp -oStrictHostKeyChecking=no -i ${var.privkey_path} -r ${var.artifacts_path} centos@${aws_instance.nvidiaDockerCentos.public_ip}:/tmp"
    }
  }

  provisioner "remote-exec" {
    inline = ["chmod +x ~/nvDocker2_install.sh && sudo ./nvDocker2_install.sh",
      "chmod +x ~/bats_install.sh && sudo ./bats_install.sh",
      "mkdir tests",
      "if ${var.artifacts}; then sudo rpm -ivh --force /tmp/x86_64/*.rpm; fi"
    ]
  }

  provisioner "file" {
    source      = "../tests/"
    destination = "~/tests"
  }

  provisioner "file" {
    source      = "../commons/run_tests.sh"
    destination = "~/run_tests.sh"
  }

  provisioner "remote-exec" {
    inline = ["chmod +x ~/run_tests.sh"]
  }
}

resource "aws_key_pair" "sshLogin" {
  key_name   = "${var.key_name}"
  public_key = "${file("${var.pubkey_path}")}"
}

output "instance_login" {
  value = "centos@${aws_instance.nvidiaDockerCentos.public_ip}"
}
provider "aws" {
	region = "${var.aws_region}"
}

resource "aws_instance" "nvidiaDockerCentos" {
	ami = "${var.ami}"
	instance_type = "${var.instance_type}"

	tags {
	     Name = "${var.instance_name}"
	}

	root_block_device {
			  volume_size = 80
	}

	key_name = "${var.key_name}"

	connection {
		   type = "ssh"
		   user = "centos"
		   private_key = "${file("${var.privkey_path}")}"
		   agent = false
		   timeout = "3m"
	}

	provisioner "file" {
		    source = "driver_install.sh"
		    destination = "~/driver_install.sh"
	}

	provisioner "file" {
		    source = "other_install.sh"
		    destination = "~/other_install.sh"
	}

	provisioner "file" {
		    source = "../commons/bats_install.sh"
		    destination = "~/bats_install.sh"
	}

	provisioner "remote-exec" {
		    inline = ["chmod +x ~/driver_install.sh && sudo ./driver_install.sh",
			   "sudo reboot"
			   ]
	}

	provisioner "remote-exec" {
		    inline = ["chmod +x ~/other_install.sh && sudo ./other_install.sh",
			   "chmod +x ~/bats_install.sh && sudo ./bats_install.sh",
			   "mkdir tests"
			   ]
	}

	provisioner "file" {
		    source = "../tests/cuda"
		    destination = "~/tests"
	}

	provisioner "file" {
		    source =   "../tests/envopts"
		    destination = "~/tests"
	}

	provisioner "file" {
		    source = "../tests/frameworks"
		    destination = "~/tests"
	}


	provisioner "file" {
		    source = "../tests/images"
		    destination = "~/tests"
	}

	provisioner "file" {
		    source = "../tests/runc"
		    destination = "~/tests"
	}

	provisioner "file" {
		    source = "../tests/runopts"
		    destination = "~/tests"
	}

	provisioner "file" {
		    source = "../tests/stress"
		    destination = "~/tests"
	}

	provisioner "file" {
		    source = "../tests/symlinks"
		    destination = "~/tests"
	}

	provisioner "file" {
		    source = "../tests/video"
		    destination = "~/tests"
	}

	provisioner "file" {
		    source = "../tests/helpers.bash"
		    destination = "~/tests/helpers.bash"
	}

	provisioner "file" {
		    "source" = "../commons/run_tests.sh"
		    destination = "~/run_tests.sh"
	}

	provisioner "remote-exec" {
		    inline = ["chmod +x ~/run_tests.sh && sudo ~/run_tests.sh"
		    	   ]
	}
}

resource "aws_key_pair" "NVDocker" {
	key_name   = "${var.key_name}"
	public_key = "${file("${var.pubkey_path}")}"
}

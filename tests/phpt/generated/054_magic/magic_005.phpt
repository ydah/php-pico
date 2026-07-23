--TEST--
magic get set and string conversion vector 005
--FILE--
<?php
class GeneratedMagic005 {
    private $value = 57;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic005(); echo $box->missing, ':';
$box->dynamic = 60; echo $box;
--EXPECT--
missing:57:box=60

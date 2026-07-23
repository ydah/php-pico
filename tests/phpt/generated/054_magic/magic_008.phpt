--TEST--
magic get set and string conversion vector 008
--FILE--
<?php
class GeneratedMagic008 {
    private $value = 90;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic008(); echo $box->missing, ':';
$box->dynamic = 93; echo $box;
--EXPECT--
missing:90:box=93

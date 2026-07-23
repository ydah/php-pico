--TEST--
magic get set and string conversion vector 006
--FILE--
<?php
class GeneratedMagic006 {
    private $value = 68;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic006(); echo $box->missing, ':';
$box->dynamic = 71; echo $box;
--EXPECT--
missing:68:box=71

--TEST--
magic get set and string conversion vector 010
--FILE--
<?php
class GeneratedMagic010 {
    private $value = 112;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic010(); echo $box->missing, ':';
$box->dynamic = 115; echo $box;
--EXPECT--
missing:112:box=115

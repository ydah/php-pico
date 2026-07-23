--TEST--
private and protected member access vector 006
--FILE--
<?php
class GeneratedVisibility006 {
    private $left = 46; protected $right = 36;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility006())->total();
--EXPECT--
82
